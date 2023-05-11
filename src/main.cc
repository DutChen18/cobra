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

cobra::future<cobra::unit> server() {
	return cobra::start_server(NULL, "25565", [](cobra::connected_socket socket) {
		std::vector<char> buffer(1024);
		
		return cobra::async_while<cobra::unit>(cobra::capture([](cobra::connected_socket& sock, std::vector<char>& buffer) {
			return sock.read(buffer.data(), buffer.size()).and_then<cobra::optional<cobra::unit>>([&sock, &buffer](std::size_t count) {
				return sock.write_all(buffer.data(), count).and_then<cobra::optional<cobra::unit>>([](std::size_t nwritten) {
					return resolve(nwritten == 0 ? cobra::some<cobra::unit>() : cobra::none<cobra::unit>());
				});
			});
		}, std::move(socket), std::move(buffer)));
	});
}

cobra::future<cobra::http_request> parse_request_from_file() {
	cobra::fstream stream("test.http", std::fstream::in);
	std::unique_ptr<cobra::buffered_istream> file = cobra::make_unique<cobra::buffered_istream>(std::move(stream));

	return cobra::parse_request(*file).and_then<cobra::http_request>(capture([](std::unique_ptr<cobra::buffered_istream>&, cobra::http_request result) {
		return resolve(std::move(result));
	}, std::move(file)));
}

int main() {
	std::unique_ptr<cobra::context> ctx = cobra::default_context();

	parse_request_from_file().start_later(*ctx, [](cobra::future_result<cobra::http_request> result) {
		if (!result) {
			std::cerr << result.err()->what() << std::endl;
		} else {
			std::cout << result->headers().at("host") << std::endl;
		}
	});

	ctx->run_until_complete();

	return 0;
}
