#include "cobra/context.hh"
#include "cobra/event_loop.hh"
#include "cobra/executor.hh"
#include "cobra/future.hh"
#include "cobra/socket.hh"

#include <iostream>
#include <memory>
#include <thread>
#include <chrono>

cobra::future<cobra::unit> server() {
	return cobra::start_server(NULL, "25565", [](cobra::connected_socket socket) {
		std::vector<char> buffer(1024);
		
		return cobra::async_while<cobra::unit>(cobra::capture([](cobra::connected_socket& sock, std::vector<char>& buffer) {
			return sock.read(buffer.data(), buffer.size()).and_then<cobra::optional<cobra::unit>>([&sock, &buffer](std::size_t count) {
				return sock.write_all(buffer.data(), count).and_then<cobra::optional<std::size_t>>([](std::size_t) {
					return resolve(cobra::some<cobra::unit>());
				});
			});
		}, std::move(socket), std::move(buffer)));
	});
}

int main() {
	std::unique_ptr<cobra::context> ctx = cobra::default_context();

	server().start_later(*ctx, [](cobra::future_result<cobra::unit> result) {
		if (!result) {
			std::cerr << result.err()->what() << std::endl;
		}
	});

	ctx->run_until_complete();

	return 0;
}
