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

#include <cstdlib>
#include <iostream>

extern "C" {
#include <sys/socket.h>
}

#ifndef COBRA_TEST

/*
cobra::task<void> pipe(cobra::buffered_istream_reference istream, cobra::ostream_reference ostream) {
	while (true) {
		auto [buffer, buffer_size] = co_await istream.fill_buf();

		if (buffer_size == 0) {
			break;
		}

		co_await ostream.write_all(buffer, buffer_size);
		co_await ostream.flush();
		istream.consume(buffer_size);
	}
}

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

thread_local std::optional<std::jthread> t_thread;
thread_local cobra::sequential_executor t_exec;
thread_local std::optional<cobra::epoll_event_loop> t_loop;

cobra::future_task<void> run(cobra::executor* exec, cobra::event_loop* loop) {
	co_return co_await cobra::start_server(exec, loop, "127.0.0.1", "8080", [](cobra::socket_stream socket) -> cobra::task<void> {
		if (!t_thread) {
			t_loop.emplace(t_exec);

			t_thread = std::jthread([&loop = t_loop]() {
				while (true) {
					loop->poll();
				}
			});
		}

		socket.set_event_loop(&*t_loop);

		cobra::istream_buffer socket_istream(cobra::make_istream_ref(socket), 1024);
		cobra::ostream_buffer socket_ostream(cobra::make_ostream_ref(socket), 1024);

		try {
			cobra::http_request request = co_await cobra::parse_http_request(socket_istream);
			cobra::http_response response({ 1, 1 }, 200, "OK");

			response.set_header("Content-Type", "text/plain");
			response.set_header("Content-Length", std::format("{}", request.uri().length()));
			response.set_header("Connection", "close");

			co_await cobra::write_http_response(socket_ostream, response);
			co_await cobra::print(socket_ostream, "{}", request.uri());
			co_await socket_ostream.flush();
		} catch (cobra::http_parse_error error) {
			cobra::eprintln("error: {}", (int) error);
		}
	});
}

int main() {
	cobra::thread_pool_executor exec(8);
	// cobra::sequential_executor exec;
	cobra::epoll_event_loop loop(exec);
	auto task = run(&exec, &loop);

	while (true) {
		loop.poll();
	}

	task.get_future().wait();
	return EXIT_SUCCESS;
}
#endif
