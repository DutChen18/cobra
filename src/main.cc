#include "cobra/asio.hh"
#include "cobra/context.hh"
#include "cobra/event_loop.hh"
#include "cobra/executor.hh"
#include "cobra/future.hh"
#include "cobra/socket.hh"
#include "cobra/http.hh"
#include "cobra/file.hh"

#include <iostream>
#include <memory>
#include <thread>
#include <chrono>

static const char *response = "HTTP/1.1 200 OK\r\nContent-Length: 15\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\nHello, World!\r\n";

cobra::future<cobra::unit> server() {
	return cobra::start_server(NULL, "25565", [](cobra::connected_socket sock) {
		std::shared_ptr<cobra::connected_socket> socket = std::make_shared<cobra::connected_socket>(std::move(sock));
		std::shared_ptr<cobra::buffered_istream> istream = std::make_shared<cobra::buffered_istream>(socket);
		std::shared_ptr<cobra::buffered_ostream> ostream = std::make_shared<cobra::buffered_ostream>(socket);

		return cobra::parse_request(*istream).and_then<cobra::unit>([socket, istream, ostream](cobra::http_request) {
			return socket->write_all(response, std::strlen(response)).and_then<cobra::unit>([socket, istream, ostream](std::size_t) {
				return cobra::resolve(cobra::unit());
			});
		});
	});
}

int main() {
	std::unique_ptr<cobra::context> ctx = cobra::default_context();

	server().start_later(*ctx, [](cobra::future_result<cobra::unit> result) {
		if (!result) {
			kill(getpid(), SIGTRAP);
			std::cerr << result.err()->what() << std::endl;
		}
	});

	ctx->run_until_complete();

	return 0;
}
