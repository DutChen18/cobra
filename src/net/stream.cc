#include "cobra/net/stream.hh"

#include "cobra/exception.hh"
#include "cobra/print.hh"
#include "cobra/net/address.hh"

#include <mutex>
#include <numeric>
#include <stdexcept>
#include <utility>

extern "C" {
#include <fcntl.h>
#include <sys/socket.h>
#include <openssl/err.h>
}

namespace cobra {
	static bool check_sock(int fd) {
		int error;
		socklen_t len = sizeof error;

		check_return(getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len));
		return error == 0;
	}

	basic_socket_stream::~basic_socket_stream() {}

	socket_stream::socket_stream(socket_stream&& other) : _loop(std::exchange(other._loop, nullptr)), _file(std::move(other._file)) {}
	socket_stream::socket_stream(event_loop* loop, file&& f) : _loop(loop), _file(std::move(f)) {}
	socket_stream::~socket_stream() {}

	task<std::size_t> socket_stream::read(char_type* data, std::size_t size) {
		co_await _loop->wait_read(_file);
		co_return check_return(recv(_file.fd(), data, size, 0));
	}

	task<std::size_t> socket_stream::write(const char_type* data, std::size_t size) {
		co_await _loop->wait_write(_file);
		co_return check_return(send(_file.fd(), data, size, 0));
	}

	task<void> socket_stream::flush() {
		co_return;
	}

	task<void> socket_stream::shutdown(int how) {
		check_return(::shutdown(_file.fd(), how));
		co_return;
	}
	
	address socket_stream::peername() const {
		sockaddr_storage addr;
		socklen_t len = sizeof addr;
		check_return(getpeername(_file.fd(), reinterpret_cast<sockaddr*>(&addr), &len));
		return address(reinterpret_cast<sockaddr*>(&addr), len);
	}

	ssl_error::ssl_error(const std::string& what) : std::runtime_error(what) {}
	ssl_error::ssl_error(std::mutex& mtx) : std::runtime_error(get_all_errors(mtx)) {}

	std::string ssl_error::get_all_errors(std::mutex& mtx) {
		std::lock_guard guard(mtx);
		return get_all_errors_unsafe();
	}

	std::string ssl_error::get_all_errors_unsafe() {
		std::string errors;

		unsigned long errc;
		while ((errc = ERR_get_error()) != 0) {
			errors.append(std::string(ERR_error_string(errc, nullptr)));
			errors.push_back('\n');
		}
		return errors;
	}

	ssl::ssl(SSL_CTX* ctx) {
		_ssl = SSL_new(ctx);
		if (_ssl == nullptr) {
			throw ssl_error();
		}
	}

	/*
	ssl::ssl(const ssl& other) : _ssl(other._ssl) {
		if (SSL_up_ref(_ssl) == 0) {
			throw ssl_error("failed to increase SSL ref count");
		}
	}*/

	ssl::ssl(ssl&& other) noexcept : _ssl(std::exchange(other._ssl, nullptr)) {}

	ssl_error::ssl_error() : std::runtime_error("something went wrong. oops"){
	}

	ssl::~ssl() {
		SSL_free(_ssl);
	}

	void ssl::set_file(const file& f) {
		if (SSL_set_fd(_ssl, f.fd()) == 0) {
			throw ssl_error("failed to set ssl fd");
		}
	}

	ssl_ctx::ssl_ctx(const config::ssl_config& cfg) : ssl_ctx(cfg.cert, cfg.key) {}

	ssl_ctx::ssl_ctx(const std::filesystem::path& cert, const std::filesystem::path& key) {
		const SSL_METHOD *method = TLS_server_method();
		if (method == nullptr) {
			throw std::runtime_error("TLS_server_method failed");
		}

		_ctx = SSL_CTX_new(method);

		if (_ctx == nullptr) {
			throw std::runtime_error("ssl_ctx_new failed");
		}

		if (SSL_CTX_use_certificate_file(_ctx, cert.c_str(), SSL_FILETYPE_PEM) <= 0) {
			//TODO fix possible leak of _ctx
			throw std::runtime_error("ssl_ctx_use_cert_file failed");
		}
		if (SSL_CTX_use_PrivateKey_file(_ctx, key.c_str(), SSL_FILETYPE_PEM) <= 0) {
			//TODO fix possible leak of _ctx
			throw std::runtime_error("ssl_ctx_use_privkey_file failed");
		}
	}

	ssl_ctx::ssl_ctx(const ssl_ctx& other) : _ctx(other._ctx) {
		//SSL_CTX_up_ref is atomic
		if (SSL_CTX_up_ref(_ctx) == 0) {
			throw ssl_error("failed to increase SSL_CTX ref count");
		}
	}

	ssl_ctx::~ssl_ctx() {
		SSL_CTX_free(_ctx);
	}

	ssl ssl_ctx::new_ssl() {
		std::lock_guard guard(_mtx);//TODO  check if this mutex is actually needed
		return ssl(_ctx);
	}

	ssl_socket_stream::ssl_socket_stream(event_loop* loop, file&& f, ssl&& ssl) : _loop(loop), _file(std::move(f)), _ssl(std::move(ssl)) {}

	ssl_socket_stream::ssl_socket_stream(ssl_socket_stream&& other) : _mtx(std::move(other._mtx)), _loop(other._loop), _file(std::move(other._file)), _ssl(std::move(other._ssl)) {
		_write_shutdown = std::exchange(other._write_shutdown, true);
		_read_shutdown = std::exchange(other._read_shutdown, true);
	}

	ssl_socket_stream::~ssl_socket_stream() {
		if (!_write_shutdown) {
			std::cerr << "ssl connection was not correctly shut down" << std::endl;
		}
	}

	task<ssl_socket_stream> ssl_socket_stream::accept(event_loop* loop, socket_stream&& socket, ssl&& _ssl) {
		_ssl.set_file(socket._file.fd());

		while (true) {
			co_await loop->wait_read(socket._file);
			int rc = SSL_accept(_ssl.leak());
			if (rc <= 0) {
				int error = SSL_get_error(_ssl.leak(), rc);
				if (rc == 0) {
					throw std::runtime_error("tls connection was shut down");
				}
				if (error == SSL_ERROR_WANT_READ) {
					continue;
				}
				throw std::runtime_error("ssl_accept error");
			}
			break;
		}

		co_return ssl_socket_stream(loop, std::move(socket._file), std::move(_ssl));
	}

	task<std::size_t> ssl_socket_stream::read(char_type* data, std::size_t size) {
		if (_read_shutdown)
			co_return 0;

		std::size_t nread = 0;

		while (true) {
			co_await _loop->wait_read(_file);
			int rc = SSL_read_ex(_ssl.leak(), data, size, &nread);
			if (rc <= 0) {
				int error = SSL_get_error(_ssl.leak(), rc);
				if (error == SSL_ERROR_WANT_READ) {
					continue;
				}
				throw std::runtime_error("ssl_read_ex error");
			}
		}
		co_return nread;
	}

	task<std::size_t> ssl_socket_stream::write(const char_type* data, std::size_t size) {
		if (_write_shutdown)
			co_return 0;

		std::size_t nwritten = 0;

		while (true) {
			co_await _loop->wait_write(_file);
			int rc = SSL_write_ex(_ssl.leak(), data, size, &nwritten);
			if (rc <= 0) {
				int error = SSL_get_error(_ssl.leak(), rc);
				if (error == SSL_ERROR_WANT_WRITE) {
					continue;
				}
				throw std::runtime_error("ssl_write_ex error");
			}

		}
		co_return nwritten;
	}

	task<void> ssl_socket_stream::flush() {
		co_return;
	}

	address ssl_socket_stream::peername() const {
		sockaddr_storage addr;
		socklen_t len = sizeof addr;
		check_return(getpeername(_file.fd(), reinterpret_cast<sockaddr*>(&addr), &len));
		return address(reinterpret_cast<sockaddr*>(&addr), len);
	}

	//TODO make `how` an enum
	task<void> ssl_socket_stream::shutdown(int how) {
		if (how == SHUT_RD || how == SHUT_RDWR) {
			shutdown_read();
		} else if (how == SHUT_WR || how == SHUT_RDWR) {
			co_await shutdown_write();
		}
	}

	void ssl_socket_stream::shutdown_read() {
		if (_read_shutdown) {
			throw std::runtime_error("read end already shut down");
		}
		_read_shutdown = true;
	}

	task<void> ssl_socket_stream::shutdown_write() {
		if (_write_shutdown) {
			throw std::runtime_error("write end already shut down");
		}
		_write_shutdown = true;

		while (true) {
			int rc = SSL_shutdown(_ssl.leak());
			
			if (rc == 1) {
				break;
			} else if (rc < 0) {
				int error = SSL_get_error(_ssl.leak(), rc);

				if (error == SSL_ERROR_WANT_READ) {
					co_await _loop->wait_read(_file);
				} else if (error == SSL_ERROR_WANT_WRITE) {
					co_await _loop->wait_write(_file);
				} else {
					throw std::runtime_error("ssl_shutdown errored");
				}
			}
		}
	}

	task<socket_stream> open_connection(event_loop* loop, const char* node, const char* service) {
		for (const address_info& info : get_address_info(node, service)) {
			file sock = check_return(socket(info.family(), info.socktype(), info.protocol()));
			check_return(fcntl(sock.fd(), F_SETFL, O_NONBLOCK));
			check_return(connect(sock.fd(), info.addr().addr(), info.addr().len()));
			co_await loop->wait_write(sock);

			if (check_sock(sock.fd())) {
				co_return socket_stream(loop, std::move(sock));
			}
		}

		throw std::runtime_error("connection failed");
	}

	task<ssl_socket_stream> open_ssl_connection(event_loop* loop, const char* node, const char* service) {
		socket_stream socket = co_await open_connection(loop, node, service);
		co_return ssl_socket_stream(loop, std::move(socket).leak(), ssl_ctx().new_ssl());
	}

	task<void> start_server(executor* exec, event_loop* loop, const char* node, const char* service,
							std::function<task<void>(socket_stream)> cb) {
		static const int val = 1;

		// TODO: should listen to all results from get_address_info
		for (const address_info& info : get_address_info(node, service)) {
			file server_sock = check_return(socket(info.family(), info.socktype(), info.protocol()));
			check_return(fcntl(server_sock.fd(), F_SETFL, O_NONBLOCK));
			check_return(setsockopt(server_sock.fd(), SOL_SOCKET, SO_REUSEADDR, &val, sizeof val));
			check_return(bind(server_sock.fd(), info.addr().addr(), info.addr().len()));
			check_return(listen(server_sock.fd(), 5));

			while (true) {
				co_await loop->wait_read(server_sock);
				sockaddr_storage addr;
				socklen_t len = sizeof addr;
				file client_sock = check_return(accept(server_sock.fd(), reinterpret_cast<sockaddr*>(&addr), &len));
				check_return(fcntl(client_sock.fd(), F_SETFL, O_NONBLOCK));
				(void) exec->schedule(cb(socket_stream(loop, std::move(client_sock))));
			}
		}
	}

	task<void> start_ssl_server(ssl_ctx ctx, executor* exec, event_loop* loop, const char* node, const char* service,
							std::function<task<void>(ssl_socket_stream)> cb) {
		co_await start_server(exec, loop, node, service, [ctx, loop, cb](socket_stream socket) mutable -> task<void> {
			co_await cb(co_await ssl_socket_stream::accept(loop, std::move(socket), ctx.new_ssl()));
		});
	}
} // namespace cobra
