#include "cobra/socket.hh"
#include "cobra/future.hh"

#include <iostream>
#include <cstring>
#include <stdexcept>

extern "C" {
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
}

namespace cobra {

	addr_info::addr_info(const std::string& host, const std::string& service) {
		addrinfo hints;

		std::memset(&hints, 0, sizeof hints);
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;

		int rc = getaddrinfo(host.c_str(), service.c_str(), &hints, &internal);

		if (rc != 0)
			throw std::runtime_error(gai_strerror(rc));
	}

	addr_info::~addr_info() {
		freeaddrinfo(internal);
	}

	socket::socket(int fd) : socket_fd(fd) {
		set_non_blocking(fd);
	}

	socket::socket(const std::string& host, const std::string& service) {
		addr_info info(host, service);

		for (auto&& x : info) {
			socket_fd = ::socket(x.ai_family, x.ai_socktype, x.ai_protocol);
			if (socket_fd != -1)
				break;
		}

		if (socket_fd == -1)
			throw std::system_error(std::make_error_code(static_cast<std::errc>(errno)));

		set_non_blocking(socket_fd);
	}

	socket::socket(socket&& other) noexcept {
		std::swap(other.socket_fd, socket_fd);
	}

	socket::~socket() {
		if (socket_fd == -1)
			return;

		//TODO write utility function for closing fds that does this
		int rc = close(socket_fd);

		if (rc == -1) {
			std::cerr << "Failed to properly close socket fd" << socket_fd << ": "
				<< std::strerror(errno) << std::endl;
		}
	}

	socket& socket::operator=(socket&& other) noexcept {
		if (this != &other) {
			socket_fd = other.socket_fd;
			other.socket_fd = -1;
		}
		return *this;
	}

	int socket::get_socket_fd() const {
		return socket_fd;
	}

	void socket::set_non_blocking(int fd) {
		int rc = fcntl(fd, F_SETFL, O_NONBLOCK);
		
		if (rc == -1)
			throw std::system_error(std::make_error_code(static_cast<std::errc>(errno)));
	}

	iosocket::iosocket(int fd, sockaddr addr, socklen_t addrlen) : socket(fd), addr(addr), addrlen(addrlen) {}
	
	iosocket::iosocket(iosocket&& other) noexcept : socket(std::move(other)) {
		addr = other.addr;
		addrlen = other.addrlen;

		memset(&other.addr, 0, other.addrlen);
		other.addrlen = 0;
	}

	iosocket& iosocket::operator=(iosocket&& other) noexcept {
		if (this != &other) {
			socket::operator=(std::move(other));

			addr = other.addr;
			addrlen = other.addrlen;

			memset(&other.addr, 0, other.addrlen);
			other.addrlen = 0;
		}
		return *this;
	}

	future<std::size_t> iosocket::read(void *, std::size_t ) {
		/*
		const int socket_fd = get_socket_fd();
		return future<std::size_t>([dst, count, socket_fd](const context<void*, std::size_t>& ctx) {
		});*/
		return 0;
	}

	future<std::size_t> write(const void*, std::size_t) {
		return 0;
	}

	server::server(const std::string& host, const std::string& service, callback_type callback, int backlog) : socket(host, service), callback(callback) {
		listen_fd = listen(get_socket_fd(), backlog);

		if (listen_fd == -1)
			throw std::system_error(std::make_error_code(static_cast<std::errc>(errno)));
	}

	server::~server() {
		if (listen_fd == -1)
			return;

		int rc = close(listen_fd);

		if (rc == -1) {
			std::cerr << "Failed to properly close socket fd" << listen_fd << ": "
				<< std::strerror(errno) << std::endl;
		}
	}

	future<> server::start() {
		int socket_fd = get_socket_fd();
		return async_while([this, socket_fd]() {
			return future<bool>([this, socket_fd](const context<bool>& ctx) {
				ctx.get_loop().on_read_ready(socket_fd, [this, ctx, socket_fd]() {
					sockaddr addr;
					socklen_t addrlen;
					int fd = accept(socket_fd, &addr, &addrlen);
					if (fd == -1)
						throw std::system_error(std::make_error_code(static_cast<std::errc>(errno)));

					iosocket socket(fd, addr, addrlen);

					auto fut = callback(std::move(socket));
					auto handler = ctx.detach();
					fut.start(handler);

					ctx.resolve(true);
				});
			});
		});
	}
}
