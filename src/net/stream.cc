#include "cobra/net/stream.hh"

#include "cobra/exception.hh"
#include "cobra/net/address.hh"

extern "C" {
#include <fcntl.h>
#include <sys/socket.h>
}

namespace cobra {
	static ssize_t check(ssize_t ret) {
		if (ret < 0 && errno != EINPROGRESS) {
			throw errno_exception();
		} else {
			return ret;
		}
	}

	static bool check_fd(int fd) {
		int error;
		socklen_t len = sizeof error;

		check(getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len));
		return error == 0;
	}

	socket_stream::socket_stream(event_loop* loop, file&& f) : _loop(loop), _file(std::move(f)) {}

	socket_stream::socket_stream(socket_stream&& other) : _loop(other._loop), _file(std::move(other._file)) {}

	task<std::size_t> socket_stream::read(char_type* data, std::size_t size) {
		co_await _loop->wait_ready(poll_type::read, _file, std::nullopt);
		co_return check(recv(_file.fd(), data, size, 0));
	}

	task<std::size_t> socket_stream::write(const char_type* data, std::size_t size) {
		co_await _loop->wait_ready(poll_type::write, _file, std::nullopt);
		co_return check(send(_file.fd(), data, size, 0));
	}

	task<void> socket_stream::flush() { co_return; }

	task<socket_stream> open_connection(event_loop* loop, const char* node, const char* service) {
		for (const address_info& info : get_address_info(node, service)) {
			file sock = check(socket(info.family(), info.socktype(), info.protocol()));
			check(fcntl(sock.fd(), F_SETFL, O_NONBLOCK));
			check(connect(sock.fd(), info.addr().addr(), info.addr().len()));
			co_await loop->wait_ready(poll_type::write, sock, std::nullopt);

			if (check_fd(sock.fd())) {
				co_return socket_stream(loop, std::move(sock));
			}
		}

		throw std::runtime_error("connection failed");
	}

	task<void> start_server(event_loop* loop, const char* node, const char* service,
							std::function<task<void>(socket_stream)> cb) {
		for (const address_info& info : get_address_info(node, service)) {
			file server_sock = check(socket(info.family(), info.socktype(), info.protocol()));
			check(fcntl(server_sock.fd(), F_SETFL, O_NONBLOCK));
			check(bind(server_sock.fd(), info.addr().addr(), info.addr().len()));
			check(listen(server_sock.fd(), 5));

			while (true) {
				co_await loop->wait_ready(poll_type::read, server_sock.fd(), std::nullopt);
				sockaddr_storage addr;
				socklen_t len = sizeof addr;
				file client_sock = check(accept(server_sock.fd(), reinterpret_cast<sockaddr*>(&addr), &len));
				check(fcntl(server_sock.fd(), F_SETFL, O_NONBLOCK));
				co_await cb(socket_stream(loop, std::move(client_sock)));
			}
		}
	}
} // namespace cobra
