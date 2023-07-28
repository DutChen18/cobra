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

#define SSL_CHECK(ssl, loop, fd, func) \
	do { \
		int rc = func; \
		if (rc <= 0) { \
			int error = SSL_get_error(ssl, rc); \
			if (error == SSL_ERROR_NONE) { \
				break; \
			} else if (error == SSL_ERROR_WANT_READ) { \
				co_await loop->wait_read(f);\
			} else if (error == SSL_ERROR_WANT_WRITE) { \
				co_await loop->wait_write(f);\
			} else { \
				throw std::runtime_error(##func " failed"); \
			} \
		} else { \
			break; \
		} \
	} while (1)

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

	task<void> socket_stream::shutdown(shutdown_how how) {
		int h = 0;
		switch (how) {
		case shutdown_how::read: 
			h = SHUT_RD;
			break;
		case shutdown_how::write: 
			h = SHUT_WR;
			break;
		case shutdown_how::both:
			h = SHUT_WR;
		}
		check_return(::shutdown(_file.fd(), h));
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

	ssl::ssl(ssl_ctx ctx) : _ctx(ctx) {
		_ssl = SSL_new(ctx._ctx);
		if (_ssl == nullptr) {
			throw ssl_error();
		}
	}

	ssl::ssl(ssl&& other) noexcept : _ssl(std::exchange(other._ssl, nullptr)), _ctx(std::move(other._ctx)) {}

	ssl_error::ssl_error() : std::runtime_error("something went wrong. oops") {}

	ssl::~ssl() {
		if (_ssl){
			SSL_free(_ssl);
		}
	}

	task<void> ssl::shutdown(event_loop* loop, file&& f) {
		co_await shutdown(loop, f);
	}

	task<void> ssl::shutdown(event_loop* loop, const file& f) {
		while (true) {
			int rc = SSL_shutdown(ptr());
			
			if (rc == 1 || rc == 0) {
				break;
			}

			if (rc < 0) {
				int error = SSL_get_error(ptr(), rc);

				if (error == SSL_ERROR_WANT_READ) {
					std::cerr << "want read" << std::endl;
					co_await loop->wait_read(f);
				} else if (error == SSL_ERROR_WANT_WRITE) {
					std::cerr << "want write" << std::endl;
					co_await loop->wait_write(f);
				} else {
					throw std::runtime_error("ssl_shutdown errored");
				}
			}
		}
		co_return;
	}

	void ssl::set_file(const file& f) {
		if (SSL_set_fd(_ssl, f.fd()) == 0) {
			throw ssl_error("failed to set ssl fd");
		}
	}

	ssl_ctx::ssl_ctx(const SSL_METHOD* method) : _ctx(SSL_CTX_new(method)) {
		if (_ctx == nullptr) {
			throw std::runtime_error("ssl_new failed");
		}
	}

	ssl_ctx::ssl_ctx(const ssl_ctx& other) : _ctx(other._ctx) {
		ref_up(_ctx);
	}

	ssl_ctx::~ssl_ctx() {
		ref_down(_ctx);
	}

	ssl_ctx& ssl_ctx::operator=(const ssl_ctx& other) {
		if (this != &other) {
			ref_down(_ctx);
			_ctx = other._ctx;
			ref_up(_ctx);
		}
		return *this;
	}

	void ssl_ctx::ref_up(SSL_CTX* ctx) {
		if (SSL_CTX_up_ref(ctx) == 0) {
			throw ssl_error("failed to increase SSL_CTX ref count");
		}
	}

	void ssl_ctx::ref_down(SSL_CTX* ctx) {
		SSL_CTX_free(ctx);
	}

	ssl_ctx ssl_ctx::server(const std::filesystem::path& cert, const std::filesystem::path& key) {
		const SSL_METHOD *method = TLS_server_method();
		if (method == nullptr) {
			throw std::runtime_error("TLS_server_method failed");
		}

		ssl_ctx ctx(method);
		if (SSL_CTX_use_certificate_file(ctx._ctx, cert.c_str(), SSL_FILETYPE_PEM) <= 0) {
			throw std::runtime_error("ssl_ctx_use_cert_file failed");
		}
		if (SSL_CTX_use_PrivateKey_file(ctx._ctx, key.c_str(), SSL_FILETYPE_PEM) <= 0) {
			throw std::runtime_error("ssl_ctx_use_privkey_file failed");
		}
		return ctx;
	}

	ssl_ctx ssl_ctx::client() {
		const SSL_METHOD* method = TLS_client_method();

		if (method == nullptr) {
			throw std::runtime_error("TLS_server_method failed");
		}
		ssl_ctx ctx(method);

		SSL_CTX_set_verify(ctx._ctx, SSL_VERIFY_PEER, NULL);
		if (SSL_CTX_set_default_verify_paths(ctx._ctx) == 0) {
			throw std::runtime_error("failed to set default certs");
		}

		SSL_CTX_set_options(ctx._ctx, SSL_OP_IGNORE_UNEXPECTED_EOF);
		return ctx;
	}

	ssl_socket_stream::ssl_socket_stream(event_loop* loop, file&& f, ssl&& ssl)
		: ssl_socket_stream(nullptr, loop, std::move(f), std::move(ssl)) {}

	ssl_socket_stream::ssl_socket_stream(executor* exec, event_loop* loop, file&& f, ssl&& ssl)
		: _exec(exec), _loop(loop), _file(std::move(f)), _ssl(std::move(ssl)), _write_shutdown(false),
		  _read_shutdown(false), _fatal_error(false) {}

	ssl_socket_stream::ssl_socket_stream(ssl_socket_stream&& other)
		: _exec(other._exec), _loop(other._loop), _file(std::move(other._file)), _ssl(std::move(other._ssl)),
		  _write_shutdown(std::exchange(other._write_shutdown, true)),
		  _read_shutdown(std::exchange(other._read_shutdown, false)),
		  _fatal_error(std::exchange(other._fatal_error, false)) {
		  }

	ssl_socket_stream::~ssl_socket_stream() {
		if (!fail() && !_write_shutdown) {
			std::cerr << "ssl connection was not correctly shut down" << std::endl;
			if (_exec) {
				auto loop = _loop;
				(void)_exec->schedule([ssl = std::move(_ssl), f = std::move(_file), loop]() mutable -> task<void> {
					co_await ssl.shutdown(loop, std::move(f));
				}());
			} else {
				std::cerr << "unable to properly shutdown. This should never happen" << std::endl;
			}
		}
	}

	task<ssl_socket_stream> ssl_socket_stream::accept(executor* exec, event_loop* loop, socket_stream&& socket,
													  ssl&& _ssl) {
		_ssl.set_file(socket._file);

		while (true) {
			co_await loop->wait_read(socket._file);
			int rc = SSL_accept(_ssl.ptr());
			if (rc <= 0) {
				int error = SSL_get_error(_ssl.ptr(), rc);
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

		co_return ssl_socket_stream(exec, loop, std::move(socket._file), std::move(_ssl));
	}

	task<ssl_socket_stream> ssl_socket_stream::connect(event_loop* loop, socket_stream&& socket, ssl&& ssl) {
		co_return co_await ssl_socket_stream::connect(nullptr, loop, std::move(socket), std::move(ssl));
	}

	task<ssl_socket_stream> ssl_socket_stream::connect(executor* exec, event_loop* loop, socket_stream&& socket,
													   ssl&& ssl) {
		ssl.set_file(socket._file);

		while (true) {
			co_await loop->wait_write(socket._file);

			int rc = SSL_connect(ssl.ptr());

			if (rc <= 0) {
				int error = SSL_get_error(ssl.ptr(), rc);

				if (rc == 0) {
					throw std::runtime_error("tls connection was shut down");
				}

				if (error == SSL_ERROR_WANT_READ) {
					co_await loop->wait_read(socket._file);
				} else if (error == SSL_ERROR_WANT_WRITE) {
					co_await loop->wait_write(socket._file);
				} else {
					println("ssl_connect: {}", ERR_error_string(ERR_get_error(), nullptr));
					throw std::runtime_error("ssl_connect error");
				}
			} else {
				break;
			}
		}
		co_return ssl_socket_stream(exec, loop, std::move(socket._file), std::move(ssl));
	}

	task<std::size_t> ssl_socket_stream::read(char_type* data, std::size_t size) {
		/*
		if (!can_read())
			co_return 0;*/

		std::size_t nread = 0;

		while (true) {
			co_await _loop->wait_read(_file);
			int rc = SSL_read_ex(_ssl.ptr(), data, size, &nread);
			if (rc <= 0) {
				int error = SSL_get_error(_ssl.ptr(), rc);
				std::cerr << "read_ex error: " << error << std::endl;

				if (error == SSL_ERROR_SSL) {//TODO also check for SSL_ERROR_SYSCALL
					_fatal_error = true;
				}

				if (error == SSL_ERROR_ZERO_RETURN) {
					break;
				} else if (error != SSL_ERROR_WANT_READ) {
					perror("ssl_read_ex perror");
					ERR_print_errors_fp(stderr);
					//std::cerr << ERR_error_string(ERR_get_error(), nullptr) << std::endl;//TODO remove
					throw std::runtime_error("ssl_read_ex error");
				}
				//TODO SSL_ERROR_WANT_READ can might actually have read some bytes,
				//So next read should happen at an offset
			} else {
				break;
			}
		}
		co_return nread;
	}

	task<std::size_t> ssl_socket_stream::write(const char_type* data, std::size_t size) {
		if (!can_write())
			co_return 0;

		std::size_t nwritten = 0;

		while (true) {
			co_await _loop->wait_write(_file);
			int rc = SSL_write_ex(_ssl.ptr(), data, size, &nwritten);
			if (rc <= 0) {
				int error = SSL_get_error(_ssl.ptr(), rc);

				if (error == SSL_ERROR_SSL) {
					_fatal_error = true;
				}

				if (error != SSL_ERROR_WANT_WRITE) {
					throw std::runtime_error("ssl_write_ex error");
				}
			} else {
				break;
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
	task<void> ssl_socket_stream::shutdown(shutdown_how how) {
		switch (how) {
		case shutdown_how::read: 
			shutdown_read();
			break;
		case shutdown_how::write: 
			co_await shutdown_write();
			break;
		case shutdown_how::both:
			shutdown_read();
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
		if (!fail()) {
			_write_shutdown = true;
			co_await _ssl.shutdown(_loop, _file);
			//::shutdown(_file.fd(), SHUT_WR);
		}
		co_return;
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

	task<ssl_socket_stream> open_ssl_connection(executor* exec, event_loop* loop, const char* node, const char* service) {
		socket_stream socket = co_await open_connection(loop, node, service);
		ssl_ctx client_ctx = ssl_ctx::client();
		ssl client(client_ctx);
		SSL_set_connect_state(client.ptr());
		if (SSL_set_tlsext_host_name(client.ptr(), node) == 0) {
			throw std::runtime_error("failed to set sni");
		}
		if (SSL_set1_host(client.ptr(), node) == 0) {
			throw std::runtime_error("set1_host failed");
		}
		co_return co_await ssl_socket_stream::connect(exec, loop, std::move(socket), std::move(client));
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
		co_await start_server(exec, loop, node, service, [ctx, exec, loop, cb](socket_stream socket) mutable -> task<void> {
			co_await cb(co_await ssl_socket_stream::accept(exec, loop, std::move(socket), ssl(ctx)));
		});
	}
} // namespace cobra
