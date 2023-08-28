#include "cobra/net/stream.hh"

#include "cobra/exception.hh"
#include "cobra/print.hh"
#include "cobra/net/address.hh"

#include <functional>
#include <memory>
#include <mutex>
#include <numeric>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <utility>

#include <cassert> // ODOT: remove

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

	connection_error::connection_error(const std::string& what) : std::runtime_error(what) {}

	basic_socket_stream::~basic_socket_stream() {}

	socket_stream::socket_stream(socket_stream&& other)
		: _loop(std::exchange(other._loop, nullptr)), _file(std::move(other._file)) {}
	socket_stream::socket_stream(event_loop* loop, file&& f) : _loop(loop), _file(std::move(f)) {}
	socket_stream::~socket_stream() {}

	task<std::size_t> socket_stream::read(char_type* data, std::size_t size) {
		co_await _loop->wait_read(_file);
		co_return check_return(recv(_file.fd(), data, size, 0));
		// auto res = check_return(recv(_file.fd(), data, size, 0));
		// eprintln("read fd={} size={} data=\"{}\"", _file.fd(), size, std::string_view(data, res));
		// co_return res;
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
			// eprintln("shutdown read {}", _file.fd());
			h = SHUT_RD;
			break;
		case shutdown_how::write: 
			// eprintln("shutdown write {}", _file.fd());
			h = SHUT_WR;
			break;
		case shutdown_how::both:
			// eprintln("shutdown read/write {}", _file.fd());
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

	std::optional<std::string_view> socket_stream::server_name() const {
		return std::nullopt;
	}

	ssl_error::ssl_error(const std::string& what, std::vector<error_type> errors)
		: std::runtime_error(what), _errors(std::move(errors)) {}
	ssl_error::ssl_error(const std::string& what) : ssl_error(what, get_all_errors()) {}

	std::vector<ssl_error::error_type> ssl_error::get_all_errors() {
		std::vector<error_type> errors;

		while (true) {
			error_type error = ERR_get_error();
			if (error == 0)
				break;
			errors.push_back(error);
		}
		return errors;
	}

	ssl::ssl(ssl_ctx ctx) : _ctx(ctx) {
		_ssl = SSL_new(ctx._ctx);
		if (_ssl == nullptr) {
			throw ssl_error("SSL_new failed");
		}
	}

	ssl::ssl(ssl&& other) noexcept : _ssl(std::exchange(other._ssl, nullptr)), _ctx(std::move(other._ctx)) {
		assert(other._ssl == nullptr);
	}

	ssl::~ssl() {
		if (_ssl){
			eprintln("freed ssl");
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
			} else if (rc < 0) {
				int error = SSL_get_error(ptr(), rc);

				if (error == SSL_ERROR_WANT_READ) {
					std::cerr << "want read" << std::endl;
					co_await loop->wait_read(f);
				} else if (error == SSL_ERROR_WANT_WRITE) {
					std::cerr << "want write" << std::endl;
					co_await loop->wait_write(f);
				} else {
					throw ssl_error("SSL_shutdown error");
				}
			} else {
				throw std::runtime_error("unknown ssl error");
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

	ssl_ctx::ssl_ctx(const ssl_ctx& other) : _ctx(other._ctx), _ext(other._ext) {
		ref_up(_ctx);
	}

	ssl_ctx::~ssl_ctx() {
		ref_down(_ctx);
	}

	ssl_ctx::tlsext_ctx::tlsext_ctx(std::unordered_map<std::string, ssl_ctx> server_names)
		: _mtx(), _server_names(std::move(server_names)) {}

	ssl_ctx::tlsext_ctx::tlsext_ctx(tlsext_ctx&& other) : _mtx(), _server_names(std::move(other._server_names)) {}

	ssl_ctx::tlsext_ctx& ssl_ctx::tlsext_ctx::operator=(tlsext_ctx&& other) {
		if (this != &other) {
			_server_names = std::move(other._server_names);
		}
		return *this;
	}

	ssl_ctx& ssl_ctx::operator=(const ssl_ctx& other) {
		if (this != &other) {
			ref_down(_ctx);
			_ctx = other._ctx;
			_ext = other._ext;
			ref_up(_ctx);
		}
		return *this;
	}

	ssl_ctx ssl_ctx::server() {
		const SSL_METHOD *method = TLS_server_method();
		if (method == nullptr) {
			throw std::runtime_error("TLS_server_method failed");
		}
		return ssl_ctx(method);
	}

	int ssl_ctx::ssl_servername_callback(SSL* s, int*, void* arg) {
		tlsext_ctx* ctx = static_cast<tlsext_ctx*>(arg);
		auto& mtx = ctx->mtx();
		const auto& server_names = ctx->server_names();

		if (server_names.empty()) {
			return SSL_TLSEXT_ERR_OK;
		}

		const char* servername = SSL_get_servername(s, TLSEXT_NAMETYPE_host_name);

		if (servername != NULL) {
			for (auto& [name, ctx] : server_names) {
				if (OPENSSL_strcasecmp(servername, name.c_str()) == 0) {
					std::lock_guard guard(mtx);
					SSL_set_SSL_CTX(s, ctx._ctx);
					return SSL_TLSEXT_ERR_OK;
				}
			}
		}
		return SSL_TLSEXT_ERR_NOACK;
	}

	void ssl_ctx::ref_up(SSL_CTX* ctx) {
		if (SSL_CTX_up_ref(ctx) == 0) {
			throw ssl_error("failed to increase SSL_CTX ref count");
		}
	}

	void ssl_ctx::ref_down(SSL_CTX* ctx) {
		SSL_CTX_free(ctx);
	}

	ssl_ctx ssl_ctx::server(std::unordered_map<std::string, ssl_ctx> server_names) {
		ssl_ctx ctx = server();

		ctx._ext = std::shared_ptr<tlsext_ctx>(new tlsext_ctx(std::move(server_names)));

		SSL_CTX_set_tlsext_servername_callback(ctx._ctx, ssl_ctx::ssl_servername_callback);
		SSL_CTX_set_tlsext_servername_arg(ctx._ctx, ctx._ext.get());

		/*
		if (SSL_CTX_use_certificate_file(ctx._ctx, "cert.pem", SSL_FILETYPE_PEM) <= 0) {
			throw std::runtime_error("ssl_ctx_use_cert_file failed");
		}
		if (SSL_CTX_use_PrivateKey_file(ctx._ctx, "key.pem", SSL_FILETYPE_PEM) <= 0) {
			throw std::runtime_error("ssl_ctx_use_privkey_file failed");
		}*/

		return ctx;
	}

	ssl_ctx ssl_ctx::server(const std::filesystem::path& cert, const std::filesystem::path& key) {
		ssl_ctx ctx = server();

		if (SSL_CTX_use_certificate_file(ctx._ctx, cert.c_str(), SSL_FILETYPE_PEM) <= 0) {
			throw ssl_error("ssl_ctx_use_cert_file failed");
			//throw std::runtime_error("ssl_ctx_use_cert_file failed");
		}
		if (SSL_CTX_use_PrivateKey_file(ctx._ctx, key.c_str(), SSL_FILETYPE_PEM) <= 0) {
			throw ssl_error("SSL_CTX_use_PrivateKey_file failed");
			//throw std::runtime_error("ssl_ctx_use_privkey_file failed");
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
		  _read_shutdown(false), _bad(false) {}

	ssl_socket_stream::ssl_socket_stream(ssl_socket_stream&& other)
		: _exec(other._exec), _loop(other._loop), _file(std::move(other._file)), _ssl(std::move(other._ssl)),
		  _write_shutdown(std::exchange(other._write_shutdown, true)),
		  _read_shutdown(std::exchange(other._read_shutdown, false)),
		  _bad(std::exchange(other._bad, false)) {}

	static task<void> destruct_ssl(ssl s, file f, event_loop* loop) {
		co_await s.shutdown(loop, std::move(f));
	}

	ssl_socket_stream::~ssl_socket_stream() {
		if (!bad() && !_write_shutdown) {
			std::cerr << "here" << std::endl;
			std::cerr << "ssl connection was not correctly shut down" << std::endl;
			if (_exec) {
				auto loop = _loop;
				/*Why doesn't this work?!
				(void)_exec->schedule([ssl = std::move(_ssl), f = std::move(_file), loop]() mutable -> task<void> {
					eprintln("cleaning up");
					co_await ssl.shutdown(loop, std::move(f));
				}());
				*/
				(void)_exec->schedule(destruct_ssl(std::move(_ssl), std::move(_file), loop));
			} else {
				std::cerr << "unable to properly shutdown. This should never happen" << std::endl;
			}
		}
	}

	task<ssl_socket_stream> ssl_socket_stream::accept(executor* exec, event_loop* loop, socket_stream&& socket,
													  ssl&& ssl) {
		ssl.set_file(socket._file);

		while (true) {
			const int rc = SSL_accept(ssl.ptr());
			if (rc > 0) {
				break;
			} else if (rc == 0) {
				throw ssl_error("TLS connectoin was shut down");
			} else {
				const int error = SSL_get_error(ssl.ptr(), rc);

				if (error ==  SSL_ERROR_WANT_READ) {
					co_await loop->wait_read(socket._file);
				} else if (error == SSL_ERROR_WANT_WRITE) {
					co_await loop->wait_write(socket._file);
				} else {
					throw ssl_error("SSL_accept error");
				}
			}
		}
		co_return ssl_socket_stream(exec, loop, std::move(socket._file), std::move(ssl));
	}

	task<ssl_socket_stream> ssl_socket_stream::connect(executor* exec, event_loop* loop, socket_stream&& socket,
													   ssl&& ssl) {
		ssl.set_file(socket._file);

		while (true) {
			const int rc = SSL_connect(ssl.ptr());

			if (rc > 0) {
				break;
			} else if (rc == 0) {
				throw ssl_error("TLS connection was shut down");
			} else {
				const int error = SSL_get_error(ssl.ptr(), rc);

				if (error == SSL_ERROR_WANT_READ) {
					co_await loop->wait_read(socket._file);
				} else if (error == SSL_ERROR_WANT_WRITE) {
					co_await loop->wait_write(socket._file);
				} else {
					throw ssl_error("SSL_connect error");
				}
			}
		}
		co_return ssl_socket_stream(exec, loop, std::move(socket._file), std::move(ssl));
	}

	task<std::size_t> ssl_socket_stream::read(char_type* data, std::size_t size) {
		std::size_t nread = 0;

		while (can_read()) {
			if (co_await rw_check(SSL_read_ex(_ssl.ptr(), data, size, &nread))) {
				break;
			}
		}
		//eprintln("read: {}", std::string_view(data, nread));
		co_return nread;
	}

	task<std::size_t> ssl_socket_stream::write(const char_type* data, std::size_t size) {
		std::size_t nwritten = 0;

		while (can_write()) {
			if (co_await rw_check(SSL_write_ex(_ssl.ptr(), data, size, &nwritten))) {
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

	std::optional<std::string_view> ssl_socket_stream::server_name() const {
		const char* name = SSL_get_servername(_ssl.ptr(), TLSEXT_NAMETYPE_host_name);

		if (name) {
			return name;
		}
		return std::nullopt;
	}

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
		if (!bad()) {
			_write_shutdown = true;
			co_await _ssl.shutdown(_loop, _file);
		}
	}

	void ssl_socket_stream::set_bad() {
		_bad = true;
		throw ssl_error("a fatal openssl error has ocurred");
	}

	task<bool> ssl_socket_stream::rw_check(int rc) {
		if (rc <= 0) {
			int error = SSL_get_error(_ssl.ptr(), rc);

			if (error == SSL_ERROR_ZERO_RETURN) {
				co_return true;
			} else if (error == SSL_ERROR_WANT_READ) {
				co_await _loop->wait_read(_file);
			} else if (error == SSL_ERROR_WANT_WRITE) {
				co_await _loop->wait_write(_file);
			} else if (error == SSL_ERROR_SSL || error == SSL_ERROR_SYSCALL) {
				set_bad();
			} else {
				throw ssl_error("ssl_write_ex/ssl_read_ex error");
			}
			co_return false;
		}
		co_return true;
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

		throw connection_error("connection failed");
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

	static task<void> start_server(executor* exec, event_loop* loop, file socket,
								   std::function<task<void>(socket_stream)> cb) {
		while (true) {
			co_await loop->wait_read(socket);
			sockaddr_storage addr;
			socklen_t len = sizeof addr;
			file client_sock = check_return(accept(socket.fd(), reinterpret_cast<sockaddr*>(&addr), &len));
			check_return(fcntl(client_sock.fd(), F_SETFL, O_NONBLOCK));
			(void)exec->schedule(cb(socket_stream(loop, std::move(client_sock))));
		}
	}

	task<void> start_server(executor* exec, event_loop* loop, const char* node, const char* service,
							std::function<task<void>(socket_stream)> cb) {
		static const int val = 1;
		std::vector<task<void>> tasks;

		bool bound = false;
		for (const address_info& info : get_address_info(node, service)) {
			file server_sock = check_return(socket(info.family(), info.socktype(), info.protocol()));
			check_return(fcntl(server_sock.fd(), F_SETFL, O_NONBLOCK));
			check_return(setsockopt(server_sock.fd(), SOL_SOCKET, SO_REUSEADDR, &val, sizeof val));
			if (bind(server_sock.fd(), info.addr().addr(), info.addr().len()) != 0)
				continue;
			if (listen(server_sock.fd(), 5) != 0)
				continue;

			(void)exec->schedule(start_server(exec, loop, std::move(server_sock), cb));
			bound = true;
			/*
			while (true) {
				co_await loop->wait_read(server_sock);
				sockaddr_storage addr;
				socklen_t len = sizeof addr;
				file client_sock = check_return(accept(server_sock.fd(), reinterpret_cast<sockaddr*>(&addr), &len));
				check_return(fcntl(client_sock.fd(), F_SETFL, O_NONBLOCK));
				(void) exec->schedule(cb(socket_stream(loop, std::move(client_sock))));
			}*/
		}
		if (!bound)
			throw std::runtime_error(std::format("Failed to listen to {}:{}: {}", node, service, strerror(errno)));
		for (auto&& task : tasks) {
			co_await task;
		}
	}

	task<void> start_ssl_server(ssl_ctx ctx, executor* exec, event_loop* loop, const char* node, const char* service,
								std::function<task<void>(ssl_socket_stream)> cb) {
		co_await start_server(
			exec, loop, node, service, [ctx, exec, loop, cb](socket_stream socket) mutable -> task<void> {
				co_await cb(co_await ssl_socket_stream::accept(exec, loop, std::move(socket), ssl(ctx)));
			});
	}

	task<void> start_ssl_server(std::unordered_map<std::string, ssl_ctx> server_names, executor* exec,
								event_loop* loop, const char* node, const char* service,
								std::function<task<void>(ssl_socket_stream)> cb) {
		ssl_ctx ctx = ssl_ctx::server(std::move(server_names));
		co_await start_server(
			exec, loop, node, service, [ctx, exec, loop, cb](socket_stream socket) mutable -> task<void> {
				co_await cb(co_await ssl_socket_stream::accept(exec, loop, std::move(socket), ssl(ctx)));
			});
	}
} // namespace cobra
