#include "cobra/socket.hh"
#include "cobra/future.hh"
#include "cobra/exception.hh"
#include "cobra/event_loop.hh"

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
		fd = std::move(other.fd);
	}

	socket::~socket() {
	}

	socket& socket::operator=(socket other) {
		std::swap(fd, other.fd);
		return *this;
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
		return future<std::size_t>([this, dst, count](context& ctx, future_func<std::size_t>& resolve) {
			ctx.on_ready(fd.get(), listen_type::read, capture([this, dst, count](future_func<std::size_t>& resolve) {
				ssize_t nread = ::recv(fd.get(), dst, count, 0);

				if (nread < 0) {
					resolve(err<std::size_t, future_error>(errno_exception()));
				} else {
					resolve(ok<std::size_t, future_error>(nread));
				}
			}, std::move(resolve)));
		});
	}

	future<std::size_t> connected_socket::write(const char_type* data, std::size_t count) {
		return future<std::size_t>([this, data, count](context& ctx, future_func<std::size_t>& resolve) {
			ctx.on_ready(fd.get(), listen_type::write, capture([this, data, count](future_func<std::size_t>& resolve) {
				ssize_t nwritten = ::send(fd.get(), data, count, 0);

				if (nwritten < 0) {
					resolve(err<std::size_t, future_error>(errno_exception()));
				} else {
					resolve(ok<std::size_t, future_error>(nwritten));
				}
			}, std::move(resolve)));
		});
	}

	future<unit> connected_socket::flush() {
		return resolve(unit());
	}

	void connected_socket::shutdown(int how) {
		if (::shutdown(fd.get(), how) == -1) {
			throw errno_exception();
		}
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
		return future<connected_socket>([this](context& ctx, future_func<connected_socket>& resolve) {
			ctx.on_ready(fd.get(), listen_type::read, capture([this](future_func<connected_socket>& resolve) {
				sockaddr_storage addr;
				socklen_t len = sizeof addr;
				int connected_fd = ::accept(fd.get(), reinterpret_cast<sockaddr*>(&addr), &len);

				if (connected_fd < 0) {
					resolve(err<connected_socket, future_error>(errno_exception()));
				} else if (fcntl(connected_fd, F_SETFL, O_NONBLOCK) == -1) {
					resolve(err<connected_socket, future_error>(errno_exception()));
				} else {
					resolve(ok<connected_socket, future_error>(connected_fd));
				}
			}, std::move(resolve)));
		});
	}
	
	initial_socket::initial_socket(int family, int type, int proto) : socket(::socket(family, type, proto)) {
		if (fd.get() < 0) {
			throw errno_exception();
		} else if (fcntl(fd.get(), F_SETFL, O_NONBLOCK) == -1) {
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
		int val = 1;

		if (::setsockopt(fd.get(), SOL_SOCKET, SO_REUSEADDR, &val, sizeof val) == -1) {
			throw errno_exception();
		}

		if (::bind(fd.get(), addr.get_addr(), addr.get_len()) == -1) {
			throw errno_exception();
		}
	}

	future<connected_socket> initial_socket::connect(const address& addr)&& {
		return future<connected_socket>(capture([addr](initial_socket& self, context& ctx, future_func<connected_socket>& resolve) {
			int result = ::connect(self.fd.get(), addr.get_addr(), addr.get_len());

			if (result == -1 && errno != EINPROGRESS) {
				resolve(err<connected_socket, future_error>(errno_exception()));
			}

			ctx.on_ready(self.fd.get(), listen_type::write, capture([](initial_socket& self, future_func<connected_socket>& resolve) {
				int error;
				socklen_t len = sizeof error;

				if (::getsockopt(self.fd.get(), SOL_SOCKET, SO_ERROR, &error, &len) == -1) {
					resolve(err<connected_socket, future_error>(errno_exception()));
				} else if (error != 0) {
					resolve(err<connected_socket, future_error>(errno_exception(error)));
				} else {
					resolve(ok<connected_socket, future_error>(self.fd.leak()));
				}
			}, std::move(self), std::move(resolve)));
		}, std::move(*this)));
	}

	server_socket initial_socket::listen(int backlog)&& {
		if (::listen(fd.get(), backlog) == -1) {
			throw errno_exception();
		}

		return server_socket(fd.leak());
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
				throw future_error("open_connection error");
			}

			const address_info& info = info_list[index++];
			initial_socket socket(info.get_family(), info.get_type(), info.get_proto());

			return std::move(socket).connect(info.get_addr()).template and_then<optional<connected_socket>>([](connected_socket socket) {
				return resolve(some<connected_socket>(std::move(socket)));
			});
		}, std::move(info_list)));
	}

	future<unit> start_server(const char* host, const char* port, function<future<unit>, connected_socket>&& callback) {
		using callback_func = function<future<unit>, connected_socket>;

		std::vector<address_info> info_list = get_address_info(host, port);
		std::size_t index = 0;

		return async_while<server_socket>(capture([index](std::vector<address_info>& info_list) mutable {
			if (index >= info_list.size()) {
				throw future_error("start_server error");
			}

			const address_info& info = info_list[index++];
			initial_socket socket(info.get_family(), info.get_type(), info.get_proto());

			socket.bind(info.get_addr());
			return resolve(some<server_socket>(std::move(socket).listen()));
		}, std::move(info_list))).template and_then<unit>(capture([](callback_func& callback, server_socket socket) {
			return async_while<unit>(capture([](callback_func& callback, server_socket& socket) {
				return socket.accept().template and_then<optional<unit>>([&callback](connected_socket socket) {
					return spawn(callback(std::move(socket))).and_then<optional<unit>>([](unit) {
						return resolve(none<unit>());
					});
				});
			}, std::move(callback), std::move(socket)));
		}, std::move(callback)));
	}
}
