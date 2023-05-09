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
	void free_delete::operator()(void* ptr) const {
		std::free(ptr);
	}

	address::address(const sockaddr* addr, std::size_t len) {
		this->addr.reset((sockaddr*) std::malloc(len));
		this->len = len;
		std::memcpy(this->addr.get(), addr, len);
	}

	address::address(const address& other) {
		addr.reset((sockaddr*) std::malloc(other.len));
		len = other.len;
		std::memcpy(addr.get(), other.addr.get(), len);
	}

	address::address(address&& other) {
		addr.reset(other.addr.release());
		len = other.len;
	}

	address& address::operator=(address other) {
		std::swap(addr, other.addr);
		std::swap(len, other.len);
		return *this;
	}

	sockaddr* address::get_addr() const {
		return addr.get();
	}

	std::size_t address::get_len() const {
		return len;
	}

	address_info::address_info(const addrinfo* info) : addr(info->ai_addr, info->ai_addrlen) {
		family = info->ai_family;
		type = info->ai_socktype;
		proto = info->ai_protocol;
	}

	int address_info::get_family() const {
		return family;
	}

	int address_info::get_type() const {
		return type;
	}

	int address_info::get_proto() const {
		return proto;
	}

	const address& address_info::get_addr() const {
		return addr;
	}

	socket::socket(int fd) {
		this->fd = fd;
	}

	socket::socket(socket&& other) {
		fd = other.leak_fd();
	}

	socket::~socket() {
		if (fd >= 0 && close(fd) != -1) {
			std::cerr << "Failed to properly close socket fd " << fd;
			std::cerr << ": " << std::strerror(errno) << std::endl;
		}
	}
	
	socket& socket::operator=(socket other) {
		std::swap(fd, other.fd);
		return *this;
	}

	int socket::leak_fd() {
		int result = fd;
		fd = -1;
		return result;
	}

	connected_socket::connected_socket(int fd) : socket(fd) {
	}

	connected_socket::connected_socket(connected_socket&& other) : socket(std::move(other)) {
	}

	connected_socket& connected_socket::operator=(connected_socket other) {
		socket::operator=(std::move(other));
		return *this;
	}
	
	future<std::size_t> connected_socket::read(char_type* dst, std::size_t count) {
		return future<std::size_t>([this, dst, count](context<std::size_t>& ctx) {
			ctx.on_ready(fd, listen_type::read, capture([this, dst, count](context<std::size_t>& ctx) {
				ssize_t nread = ::recv(fd, dst, count, 0);

				if (nread < 0) {
					throw errno_exception(); // TODO: reject
				} else {
					ctx.resolve(nread);
				}
			}, std::move(ctx)));
		});
	}

	future<std::size_t> connected_socket::write(const char_type* data, std::size_t count) {
		return future<std::size_t>([this, data, count](context<std::size_t>& ctx) {
			ctx.on_ready(fd, listen_type::write, capture([this, data, count](context<std::size_t>& ctx) {
				ssize_t nread = ::write(fd, data, count);

				if (nread < 0) {
					throw errno_exception(); // TODO: reject
				} else {
					ctx.resolve(nread);
				}
			}, std::move(ctx)));
		});
	}

	future<> connected_socket::flush() {
		return future<>();
	}

	server_socket::server_socket(int fd) : socket(fd) {
	}

	server_socket::server_socket(server_socket&& other) : socket(std::move(other)) {
	}

	server_socket& server_socket::operator=(server_socket other) {
		socket::operator=(std::move(other));
		return *this;
	}

	future<connected_socket> server_socket::accept() {
		return future<connected_socket>([this](context<connected_socket>& ctx) {
			ctx.on_ready(fd, listen_type::read, capture([this](context<connected_socket>& ctx) {
				sockaddr_storage addr;
				socklen_t len;
				int connected_fd = ::accept(fd, reinterpret_cast<sockaddr*>(&addr), &len);

				if (connected_fd < 0) {
					throw errno_exception(); // TODO: reject
				} else if (fcntl(connected_fd, F_SETFL, O_NONBLOCK) == -1) {
					throw errno_exception(); // TODO: reject
				} else {
					ctx.resolve(connected_socket(connected_fd));
				}
			}, std::move(ctx)));
		});
	}
	
	initial_socket::initial_socket(int family, int type, int proto) : socket(::socket(family, type, proto)) {
		if (fd < 0) {
			throw errno_exception();
		} else if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1) {
			throw errno_exception();
		}
	}

	initial_socket::initial_socket(initial_socket&& other) : socket(std::move(other)) {
	}

	initial_socket& initial_socket::operator=(initial_socket other) {
		socket::operator=(std::move(other));
		return *this;
	}

	void initial_socket::bind(const address& addr) {
		if (::bind(fd, addr.get_addr(), addr.get_len()) == -1) {
			throw errno_exception();
		}
	}

	future<connected_socket> initial_socket::connect(const address& addr)&& {
		return future<connected_socket>(capture([addr](initial_socket& self, context<connected_socket>& ctx) {
			int result = ::connect(self.fd, addr.get_addr(), addr.get_len());

			if (result == -1 && errno != EINPROGRESS) {
				throw errno_exception(); // TODO: reject;
			}

			ctx.on_ready(self.fd, listen_type::write, capture([](initial_socket& self, context<connected_socket>& ctx) {
				int error;
				socklen_t len = sizeof error;

				if (::getsockopt(self.fd, SOL_SOCKET, SO_ERROR, &error, &len) == -1) {
					throw errno_exception(); // TODO: reject
				} else if (error != 0) {
					throw std::runtime_error("socket error"); // TODO: reject
				} else {
					ctx.resolve(connected_socket(self.leak_fd()));
				}
			}, std::move(self), std::move(ctx)));
		}, std::move(*this)));
	}

	server_socket initial_socket::listen(int backlog)&& {
		::listen(fd, backlog);
		return server_socket(leak_fd());
	}
	
	std::vector<address_info> get_address_info(const char* host, const char* port, int family, int type) {
		addrinfo hints;
		addrinfo* info;

		std::memset(&hints, 0, sizeof hints);
		hints.ai_family = family;
		hints.ai_socktype = type;
		hints.ai_flags = 0;

		if (host == nullptr) {
			hints.ai_flags |= AI_PASSIVE;
		}

		int rc = getaddrinfo(host, port, &hints, &info);

		if (rc != 0) {
			throw std::runtime_error(gai_strerror(rc));
		}

		std::vector<address_info> result;

		for (addrinfo* i = info; i != nullptr; i = i->ai_next) {
			result.push_back(i);
		}

		freeaddrinfo(info);

		return result;
	}
	
	future<connected_socket> open_connection(const char* host, const char* port) {
		std::vector<address_info> info_list = get_address_info(host, port);
		std::size_t index = 0;

		return async_while<connected_socket>(capture([index](std::vector<address_info>& info_list) mutable {
			if (index >= info_list.size()) {
				// TODO: throw
			}

			const address_info& info = info_list[index++];
			initial_socket socket(info.get_family(), info.get_type(), info.get_proto());

			return std::move(socket).connect(info.get_addr()).then([](connected_socket& socket) {
				return some(std::move(socket));
			});
		}, std::move(info_list)));
	}

	future<> start_server(const char* host, const char* port, function<future<>, connected_socket>&& callback) {
		std::vector<address_info> info_list = get_address_info(host, port);
		std::size_t index = 0;

		return async_while<server_socket>(capture([index](std::vector<address_info>& info_list) mutable {
			if (index >= info_list.size()) {
				// TODO: throw
			}

			const address_info& info = info_list[index++];
			initial_socket socket(info.get_family(), info.get_type(), info.get_proto());

			socket.bind(info.get_addr());
			return future<server_socket>(some(std::move(socket).listen()));
		}, std::move(info_list))).then([](server_socket& socket) {
			return async_while<unit>(capture([](server_socket& socket) {
				return socket.accept().then([](connected_socket& socket) {
					// TODO: call back

					return some<unit>();
				});
			}, std::move(socket)));
		}).ignore();
	}
}
