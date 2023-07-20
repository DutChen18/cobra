#include "cobra/asyncio/event_loop.hh"
#include "cobra/asyncio/executor.hh"
#include "cobra/asyncio/future_task.hh"
#include "cobra/asyncio/stream.hh"
#include "cobra/asyncio/stream_buffer.hh"
#include "cobra/asyncio/std_stream.hh"
#include "cobra/http/writer.hh"
#include "cobra/http/parse.hh"
#include "cobra/net/stream.hh"
#include "cobra/process.hh"
#include "cobra/print.hh"
#include "cobra/config.hh"
#include "cobra/http/server.hh"

#include <cstdlib>
#include <iostream>
#include <fstream>

extern "C" {
#include <sys/socket.h>
}

#ifndef COBRA_TEST

/*
cobra::future_task<void> run(cobra::executor* exec, cobra::event_loop* loop) {
	co_return co_await cobra::start_server(exec, loop, "localhost", "8080", [exec, loop](cobra::socket_stream socket) -> cobra::task<void> {
		cobra::command command({ "/usr/bin/base64" });
		command.in(cobra::command_stream_mode::pipe);
		command.out(cobra::command_stream_mode::pipe);
		cobra::process process = command.spawn(loop);

		cobra::istream_buffer socket_istream(cobra::make_istream_ref(socket), 1024);
		cobra::ostream_buffer socket_ostream(cobra::make_ostream_ref(socket), 1024);
		cobra::istream_buffer process_istream(cobra::make_istream_ref(process.out()), 1024);
		cobra::ostream_buffer process_ostream(cobra::make_ostream_ref(process.in()), 1024);

		auto socket_reader = exec->schedule([](auto& socket_istream, auto& process_ostream) -> cobra::task<void> {
			co_await pipe(socket_istream, process_ostream);
			process_ostream.inner().ptr()->close();
		}(socket_istream, process_ostream));

		auto process_reader = exec->schedule([](auto& process_istream, auto& socket_ostream) -> cobra::task<void> {
			co_await pipe(process_istream, socket_ostream);
			socket_ostream.inner().ptr()->shutdown(SHUT_WR);
		}(process_istream, socket_ostream));

		co_await socket_reader;
		co_await process_reader;
		co_await process.wait();
	});
}
*/

cobra::command get_cgi_command(const cobra::http_request& request) {
	std::string script_name = "/home/codam/cobra/cgi/index.php";
	std::string cgi_name = "/usr/bin/php-cgi";

	cobra::command command({ cgi_name, script_name });

	command.in(cobra::command_stream_mode::pipe);
	command.out(cobra::command_stream_mode::pipe);

	command.env("REQUEST_METHOD", request.method());
	command.env("SCRIPT_FILENAME", script_name);
	command.env("REDIRECT_STATUS", "200");

	if (request.has_header("Content-Length"))
		command.env("CONTENT_LENGTH", request.header("Content-Length"));
	if (request.has_header("Content-Type"))
		command.env("CONTENT_TYPE", request.header("Content-Type"));

	for (const auto& header : request.header_map()) {
		std::string key = "HTTP_";

		for (char ch : header.first) {
			key.push_back(ch == '-' ? '_' : std::toupper(ch));
		}

		command.env(key, header.second);
	}

	return command;
}

cobra::future_task<void> run(cobra::executor* exec, cobra::event_loop* loop) {
	co_return co_await cobra::start_server(exec, loop, "127.0.0.1", "8080", [exec, loop](cobra::socket_stream socket) -> cobra::task<void> {
		cobra::istream_buffer socket_istream(cobra::make_istream_ref(socket), 1024);
		cobra::ostream_buffer socket_ostream(cobra::make_ostream_ref(socket), 1024);
		cobra::http_request request = co_await cobra::parse_http_request(socket_istream);
		cobra::process process = get_cgi_command(request).spawn(loop);
		cobra::istream_buffer process_istream(cobra::make_istream_ref(process.out()), 1024);
		cobra::ostream_buffer process_ostream(cobra::make_ostream_ref(process.in()), 1024);

		auto socket_reader = exec->schedule([](auto& socket_istream, auto& process_ostream) -> cobra::task<void> {
			co_await cobra::pipe(cobra::buffered_istream_reference(socket_istream), cobra::ostream_reference(process_ostream));
			process_ostream.inner().ptr()->close();
		}(socket_istream, process_ostream));

		auto process_reader = exec->schedule([](auto& process_istream, auto& socket_ostream) -> cobra::task<void> {
			co_await cobra::parse_cgi(process_istream);
			cobra::http_response response(HTTP_OK);
			response.set_header("Connection", "close");
			co_await cobra::write_http_response(socket_ostream, response);
			co_await cobra::pipe(cobra::buffered_istream_reference(process_istream), cobra::ostream_reference(socket_ostream));
			socket_ostream.inner().ptr()->shutdown(SHUT_WR);
		}(process_istream, socket_ostream));

		co_await socket_reader;
		co_await process_reader;
		co_await process.wait();
	});
}

int main(int argc, char **argv) {
	if (argc == 1) {
		cobra::thread_pool_executor exec;
		cobra::epoll_event_loop loop(exec);
		auto task = run(&exec, &loop);

		while (true) {
			loop.poll();
		}

		task.get_future().wait();
		return EXIT_SUCCESS;
	} else {
		using namespace cobra;
		std::fstream stream(argv[1], std::ios::in);
		config::parse_session session(stream);

		try {
			config::server_config config = config::server_config::parse(session);
			config::server srv = std::move(config).commit();
			std::vector<server> servers = server::convert(srv);

			cobra::thread_pool_executor exec;
			cobra::epoll_event_loop loop(exec);

			std::vector<future_task<void>> jobs;

			for (auto&& server : servers) {
				jobs.push_back(make_future_task(server.start(&exec, &loop)));
			}

			while (true) {
				loop.poll();
			}

			for (auto&& job : jobs) {
				job.get_future().wait();
			}
		} catch (const config::error& err) {
			err.diag().print(std::cerr, session.lines());
		}
	}
}
#endif
