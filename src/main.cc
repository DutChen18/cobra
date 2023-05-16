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
#include "cobra/path.hh"

#include <iostream>
#include <sstream>
#include <memory>
#include <thread>
#include <chrono>

#ifndef COBRA_FUZZ

cobra::future<cobra::unit> server() {
	return cobra::start_server(NULL, "8080", [](cobra::connected_socket sock) {
		std::shared_ptr<cobra::connected_socket> socket = std::make_shared<cobra::connected_socket>(std::move(sock));
		std::shared_ptr<cobra::buffered_istream> istream = std::make_shared<cobra::buffered_istream>(socket);
		std::shared_ptr<cobra::buffered_ostream> ostream = std::make_shared<cobra::buffered_ostream>(socket);

		return cobra::parse_request(*istream).and_then<cobra::unit>([socket, istream, ostream](cobra::http_request request) {
			std::shared_ptr<std::string> body = std::make_shared<std::string>(request.request_uri());
			cobra::http_response response(cobra::http_version(1, 1), 200, "OK");

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

cobra::command get_cgi_command(const cobra::http_request& request) {
	std::string script_name = cobra::path("/home/codam/cobra/cgi").mount(request.request_uri());
	std::string cgi_name = "/usr/bin/php-cgi";

	if (request.request_uri().substr(request.request_uri().length() - 3) == ".py") {
		cgi_name = "/usr/bin/python";
	}
	
	cobra::command command({ cgi_name, script_name });

	command.set_in(cobra::fd_mode::pipe);
	command.set_out(cobra::fd_mode::pipe);

	command.insert_env("REQUEST_METHOD", request.method());
	command.insert_env("SCRIPT_FILENAME", script_name);
	command.insert_env("REDIRECT_STATUS", "200");

	if (request.headers().contains("Content-Length"))
		command.insert_env("CONTENT_LENGTH", request.headers().at("Content-Length"));
	if (request.headers().contains("Content-Type"))
		command.insert_env("CONTENT_TYPE", request.headers().at("Content-Type"));

	for (const auto& header : request.headers()) {
		std::string key = "HTTP_";

		for (char ch : header.first) {
			key.push_back(ch == '-' ? '_' : std::toupper(ch));
		}

		command.insert_env(key, header.second);
	}

	return command;
}

cobra::future<cobra::unit> cgi() {
	return cobra::start_server(NULL, "8080", [](cobra::connected_socket sock) {
		std::shared_ptr<cobra::connected_socket> socket = std::make_shared<cobra::connected_socket>(std::move(sock));
		std::shared_ptr<cobra::buffered_istream> socket_istream = std::make_shared<cobra::buffered_istream>(socket);
		std::shared_ptr<cobra::buffered_ostream> socket_ostream = std::make_shared<cobra::buffered_ostream>(socket);

		return cobra::parse_request(*socket_istream).and_then<cobra::unit>([socket, socket_istream, socket_ostream](cobra::http_request request) {
			cobra::command command = get_cgi_command(request);
			std::shared_ptr<cobra::process> process = std::make_shared<cobra::process>(command.spawn());
			std::shared_ptr<cobra::buffered_istream> process_istream = std::make_shared<cobra::buffered_istream>(std::make_shared<cobra::fd_istream>(std::move(process->out)));
			std::shared_ptr<cobra::buffered_ostream> process_ostream = std::make_shared<cobra::buffered_ostream>(std::make_shared<cobra::fd_ostream>(std::move(process->in)));

			return cobra::all(
				cobra::pipe(*socket_istream, *process_ostream),
				cobra::parse_cgi_headers(*process_istream).and_then<cobra::unit>([socket, process_istream, socket_ostream](cobra::header_map) {
					cobra::http_response response(cobra::http_version(1, 1), 200, "OK");

					response.headers().insert_or_assign("Connection", "close");

					return cobra::write_response(*socket_ostream, response).and_then<cobra::unit>([socket, process_istream, socket_ostream](cobra::unit) {
						return cobra::pipe(*process_istream, *socket_ostream).and_then<cobra::unit>([socket](cobra::unit) {
							socket->shutdown(SHUT_WR);

							return cobra::resolve(cobra::unit());
						});
					});
				})
			).and_then<cobra::unit>([process, socket_istream, socket_ostream, process_istream, process_ostream](std::tuple<cobra::unit, cobra::unit>) {
				return process->wait().and_then<cobra::unit>([process, socket_istream, socket_ostream, process_istream, process_ostream](int) {
					return cobra::resolve(cobra::unit());
				});
			});
		});
	});
}

int main() {
	std::unique_ptr<cobra::context> ctx = cobra::default_context();

	cgi().start_later(*ctx, [](cobra::future_result<cobra::unit> result) {
		if (!result) {
			std::cerr << result.err()->what() << std::endl;
		}
	});

	ctx->run_until_complete();

	return 0;
}
#endif
