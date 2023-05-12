#include "cobra/asio.hh"
#include "cobra/context.hh"
#include "cobra/event_loop.hh"
#include "cobra/executor.hh"
#include "cobra/future.hh"
#include "cobra/optional.hh"
#include "cobra/process.hh"
#include "cobra/socket.hh"
#include "cobra/http.hh"
#include "cobra/file.hh"

#include <iostream>
#include <memory>
#include <thread>
#include <chrono>

cobra::future<cobra::unit> server() {
	return cobra::start_server(NULL, "8080", [](cobra::connected_socket sock) {
		std::shared_ptr<cobra::connected_socket> socket = std::make_shared<cobra::connected_socket>(std::move(sock));
		std::shared_ptr<cobra::buffered_istream> istream = std::make_shared<cobra::buffered_istream>(socket);
		std::shared_ptr<cobra::buffered_ostream> ostream = std::make_shared<cobra::buffered_ostream>(socket);

		return cobra::parse_request(*istream).and_then<cobra::unit>([socket, istream, ostream](cobra::http_request request) {
			std::shared_ptr<std::string> body = std::make_shared<std::string>(request.request_uri());
			cobra::http_response response(cobra::http_version(1, 1), cobra::http_status_code::ok);

			response.headers().insert_or_assign("Content-Length", std::to_string(body->length()));
			response.headers().insert_or_assign("Content-Type", "text/plain");
			response.headers().insert_or_assign("Connection", "close");

			return cobra::write_response(*ostream, response).and_then<cobra::unit>([body, ostream](cobra::unit) {
				return ostream->write_all(body->data(), body->length()).and_then<cobra::unit>([body, ostream](std::size_t) {
					return ostream->flush().and_then<cobra::unit>([ostream](cobra::unit) {
						return cobra::resolve(cobra::unit());
					});
				});
			});
		});
	});
}

cobra::future<cobra::unit> proxy() {
	return cobra::start_server(NULL, "8080", [](cobra::connected_socket sock) {
		std::shared_ptr<cobra::connected_socket> socket = std::make_shared<cobra::connected_socket>(std::move(sock));
		std::shared_ptr<cobra::buffered_istream> istream = std::make_shared<cobra::buffered_istream>(socket);
		std::shared_ptr<cobra::buffered_ostream> ostream = std::make_shared<cobra::buffered_ostream>(socket);

		return cobra::parse_request(*istream).and_then<cobra::unit>([socket, istream, ostream](cobra::http_request request) {
			std::shared_ptr<cobra::process> process = std::make_shared<cobra::process>(cobra::command({request.request_uri()}).spawn());

			return cobra::write_request(*process, request).and_then<cobra::unit>([socket, ostream, process](cobra::unit) {
				std::shared_ptr<std::string> body = std::make_shared<std::string>();

				return cobra::async_while<cobra::unit>([body, process]() {
					return process->get().and_then<cobra::optional<cobra::unit>>([body, process](cobra::optional<int> ch) {
						if (!ch) {
							return process->wait().and_then<cobra::optional<cobra::unit>>([process](int) {
								return cobra::resolve(cobra::some<cobra::unit>());
							});
						} else {
							body->push_back(*ch);
							return cobra::resolve(cobra::none<cobra::unit>());
						}
					});
				}).and_then<cobra::unit>([body, ostream](cobra::unit) {
					cobra::http_response response(cobra::http_version(1, 1), cobra::http_status_code::ok);

					response.headers().insert_or_assign("Content-Length", std::to_string(body->length()));
					response.headers().insert_or_assign("Content-Type", "text/plain");
					response.headers().insert_or_assign("Connection", "close");

					return cobra::write_response(*ostream, response).and_then<cobra::unit>([body, ostream](cobra::unit) {
						return ostream->write_all(body->data(), body->length()).and_then<cobra::unit>([body, ostream](std::size_t) {
							return ostream->flush().and_then<cobra::unit>([ostream](cobra::unit) {
								return cobra::resolve(cobra::unit());
							});
						});
					});
				});
			});
		});
	});
}

int main() {
	std::unique_ptr<cobra::context> ctx = cobra::default_context();

	proxy().start_later(*ctx, [](cobra::future_result<cobra::unit> result) {
		if (!result) {
			std::cerr << result.err()->what() << std::endl;
		}
	});

	ctx->run_until_complete();

	return 0;
}
