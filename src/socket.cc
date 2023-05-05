#include "cobra/socket.hh"
#include "cobra/future.hh"
#include "cobra/exception.hh"

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

	addr_info::addr_info_iterator::addr_info_iterator() : ptr(nullptr) {}
	addr_info::addr_info_iterator::addr_info_iterator(addrinfo* ptr) : ptr(ptr) {}
	addr_info::addr_info_iterator::addr_info_iterator(const addr_info_iterator& other) : ptr(other.ptr) {}

	addr_info::addr_info_iterator& addr_info::addr_info_iterator::operator++() {
		ptr = ptr->ai_next;
		return *this;
	}

	addr_info::addr_info_iterator addr_info::addr_info_iterator::operator++(int) {
		addr_info_iterator old = *this;
		ptr = ptr->ai_next;
		return old;
	}

	bool addr_info::addr_info_iterator::operator==(const addr_info_iterator &other) const {
		return ptr == other.ptr;
	}

	bool addr_info::addr_info_iterator::operator!=(const addr_info_iterator &other) const {
		return !(*this == other);
	}

	const addrinfo& addr_info::addr_info_iterator::operator*() const {
		return *ptr;
	}

	const addrinfo* addr_info::addr_info_iterator::operator->() const {
		return ptr;
	}

	socket::socket(int fd, sockaddr_storage addr, socklen_t addrlen) : socket_fd(fd), addr(addr), addrlen(addrlen) {
		set_non_blocking(fd);
	}

	socket::socket(const std::string& host, const std::string& service) {
		addr_info info(host, service);

		for (auto&& x : info) {
			socket_fd = ::socket(x.ai_family, x.ai_socktype, x.ai_protocol);
			if (socket_fd == -1)
				continue;

			int rc = bind(socket_fd, x.ai_addr, x.ai_addrlen);
			if (rc == 0) {
				std::memcpy(&addr, x.ai_addr, x.ai_addrlen);
				addrlen = x.ai_addrlen;
				break;
			}

			rc = close(socket_fd);
			if (rc == 0) {
				std::cerr << "Failed to properly close socket fd" << socket_fd << ": "
					<< std::strerror(errno) << std::endl;
			}
		}

		if (socket_fd == -1)
			throw errno_exception();

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
			throw errno_exception();
	}

	iosocket::iosocket(int fd, sockaddr_storage addr, socklen_t addrlen) : socket(fd) {}
	
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

	future<std::size_t> iosocket::read_some(void *dst, std::size_t count) {
		return future<std::size_t>([this, dst, count](context<std::size_t> ctx) {
			ctx.get_loop().on_read_ready(get_socket_fd(), [this, dst, count, ctx]() {
				ssize_t nread = recv(get_socket_fd(), dst, count, 0);

				if (nread == -1)
					throw errno_exception();
				ctx.resolve(static_cast<std::size_t>(nread));
			});
		});
	}

	future<std::size_t> iosocket::write(const void* data, std::size_t count) {
		std::shared_ptr<std::size_t> progress = std::make_shared<std::size_t>(0);
		return async_while([this, progress, data, count]() {
			return future<bool>([this, progress, data, count](context<bool> ctx) {
				ctx.get_loop().on_write_ready(get_socket_fd(), [this, progress, data, count, ctx]() {
					ssize_t nwritten = send(get_socket_fd(), (const char *) data + *progress, count - *progress, 0);

					if (nwritten == -1)
						throw errno_exception();
					*progress += nwritten;

					if (*progress == count)
						ctx.resolve(false);
					else
						ctx.resolve(true);
				});
			});
		}).then<std::size_t>([progress]() {
			return *progress;
		});
	}

	server::server(const std::string& host, const std::string& service, callback_type callback, int backlog)
		: socket(host, service), callback(callback) {
		listen_fd = listen(get_socket_fd(), backlog);

		if (listen_fd == -1)
			throw errno_exception();
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
		return async_while([this]() {
			return future<bool>([this](context<bool> ctx) {
				ctx.get_loop().on_read_ready(get_socket_fd(), [this, ctx]() {
					sockaddr_storage addr;
					socklen_t addrlen;
					int fd = accept(get_socket_fd(), reinterpret_cast<sockaddr*>(&addr), &addrlen);
					if (fd == -1)
						throw errno_exception();

					iosocket socket(fd, addr, addrlen);

					auto fut = callback(std::move(socket));
					auto handler = ctx.detach();
					fut.run(handler);

					ctx.resolve(true);
				});
			});
		});
	}
}
