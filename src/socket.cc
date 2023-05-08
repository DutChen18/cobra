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

			int yes = 1;
			if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1)
				throw errno_exception();

			int rc = bind(socket_fd, x.ai_addr, x.ai_addrlen);
			if (rc == 0) {
				std::memcpy(&addr, x.ai_addr, x.ai_addrlen);
				addrlen = x.ai_addrlen;
				break;
			}

			rc = close(socket_fd);
			if (rc == 0) {
				std::cerr << "Failed to properly close socket fd " << socket_fd << ": "
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

		std::cout << "closing socket: " << socket_fd << std::endl;

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

	iosocket::iosocket(int fd, sockaddr_storage addr, socklen_t addrlen) : socket(fd, addr, addrlen) {}

	iosocket::iosocket(iosocket&& other) noexcept : socket(std::move(other)) {}
	
	future<std::size_t> iosocket::read_some(void *dst, std::size_t count) {
		int fd = get_socket_fd();

		return future<std::size_t>([fd, dst, count](context<std::size_t>& ctx) {
			ctx.on_ready(fd, listen_type::read, capture([fd, dst, count](context<std::size_t>& ctx) {
				ssize_t nread = recv(fd, dst, count, 0);

				if (nread < 0)
					throw errno_exception();

				std::move(ctx).resolve(static_cast<std::size_t>(nread));
			}, std::move(ctx)));
		});
	}

	future<std::size_t> iosocket::write(const void* data, std::size_t count) {
		int fd = get_socket_fd();

		// TODO: shared_ptr niet meer nodig
		std::shared_ptr<std::size_t> progress = std::make_shared<std::size_t>(0);

		return async_while([fd, progress, data, count]() {
			return future<bool>([fd, progress, data, count](context<bool>& ctx) {
				ctx.on_ready(fd, listen_type::write, capture([fd, progress, data, count](context<bool>& ctx) {
					ssize_t nwritten = send(fd, (const char *) data + *progress, count - *progress, 0);

					if (nwritten == -1)
						throw errno_exception();
					*progress += nwritten;

					if (*progress == count)
						ctx.resolve(false);
					else
						ctx.resolve(true);
				}, std::move(ctx)));
			});
		}).map<std::size_t>([progress]() {
			return *progress;
		});
	}

	server::server(const std::string& host, const std::string& service, callback_type&& callback, int backlog)
		: socket(host, service), callback(std::move(callback)) {
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
			return future<bool>([this](context<bool>& ctx) {
				ctx.on_ready(get_socket_fd(), listen_type::read, capture([this](context<bool>& ctx) {
					sockaddr_storage addr;
					socklen_t addrlen;
					int fd = accept(get_socket_fd(), reinterpret_cast<sockaddr*>(&addr), &addrlen);
					if (fd == -1)
						throw errno_exception();

					std::cout << "new connection on fd: " << fd << std::endl;
					iosocket socket(fd, addr, addrlen);

					auto fut = callback(std::move(socket));
					std::move(fut).run(ctx.detach());

					ctx.resolve(true);
				}, std::move(ctx)));
			});
		});
	}
}
