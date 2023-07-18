#include "cobra/net/stream.hh"

#include "cobra/exception.hh"
#include "cobra/print.hh"
#include "cobra/net/address.hh"

extern "C" {
#include <fcntl.h>
#include <sys/socket.h>
}

namespace cobra {
	static bool check_sock(int fd) {
		int error;
		socklen_t len = sizeof error;

		check_return(getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len));
		return error == 0;
	}

	socket_stream::socket_stream(event_loop* loop, file&& f) : _loop(loop), _file(std::move(f)) {}

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

	void socket_stream::shutdown(int how) {
		check_return(::shutdown(_file.fd(), how));
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

			char host[1024];
			char serv[1024];
			getnameinfo(info.addr().addr(), info.addr().len(), host, sizeof host, serv, sizeof serv, 0);
			cobra::println("{}:{}", host, serv);

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
} // namespace cobra
