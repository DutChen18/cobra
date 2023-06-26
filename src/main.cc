#include "cobra/asyncio/sync_task.hh"
#include "cobra/event_loop.hh"
#include "cobra/net/stream.hh"

#include <cstdlib>
#include <format>
#include <iostream>

cobra::sync_task<void> run(cobra::event_loop* loop) {
	cobra::socket_stream socket = co_await cobra::open_connection(loop, "localhost", "8080");
	co_await socket.write_all("Hello, World!", 13);
}

int main() {
	cobra::epoll_event_loop loop;
	cobra::sync_task<void> task = run(&loop);
	while (true)
		loop.poll();
	task.get_future().get();
	return EXIT_SUCCESS;
}
