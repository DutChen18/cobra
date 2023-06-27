#include "cobra/asyncio/future_task.hh"
#include "cobra/asyncio/async_task.hh"
#include "cobra/asyncio/task.hh"
#include "cobra/asyncio/event_loop.hh"
#include "cobra/asyncio/executor.hh"
#include "cobra/net/stream.hh"

#include <cstdlib>
#include <format>
#include <iostream>

/*
cobra::future_task<void> run(cobra::event_loop* loop) {
	cobra::socket_stream socket = co_await cobra::open_connection(loop, "localhost", "8080");
	co_await socket.write_all("Hello, World!", 13);
}

int main() {
	cobra::epoll_event_loop loop;
	cobra::future_task<void> task = run(&loop);
	while (true)
		loop.poll();
	task.get_future().get();
	return EXIT_SUCCESS;
}
*/

cobra::task<void> print(const char *str) {
	std::cout << str << std::endl;
	co_return;
}

cobra::future_task<void> run(cobra::executor* exec) {
	auto t1 = exec->schedule_task(print("a"));
	auto t2 = exec->schedule_task(print("b"));
	co_await t1;
	co_await t2;
}

int main() {
	cobra::sequential_executor exec;
	auto task = run(&exec);
	task.get_future().wait();
	return EXIT_SUCCESS;
}
