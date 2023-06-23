#include "cobra/event_loop.hh"
#include "cobra/net/stream.hh"

#include <iostream>
#include <format>
#include <cstdlib>

cobra::task<void> run(cobra::event_loop* loop) {
	cobra::socket_stream socket = co_await cobra::open_connection(loop, "localhost", "8080");
	std::cerr << "OPENED a socket" << std::endl;
	co_await socket.write_all("Hello, World!", 13);
}

int main() {
	cobra::epoll_event_loop loop;
	cobra::task<void> task = run(&loop);
	while (true) loop.poll();
	return EXIT_SUCCESS;
}
