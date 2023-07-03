#include "cobra/asyncio/async_task.hh"
#include "cobra/asyncio/executor.hh"
#include "cobra/asyncio/future_task.hh"
#include "cobra/asyncio/task.hh"
#include "cobra/asyncio/stream.hh"

#include <chrono>
#include <cstdlib>
#include <format>
#include <iostream>
#include <thread>

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

cobra::task<void> print(const char* str) {
	std::cout << str << std::endl;
	co_return;
}

template <class Rep, class Period>
cobra::task<void> sleep(const std::chrono::duration<Rep, Period>& duration) {
	std::this_thread::sleep_for(duration);
	co_return;
}

#ifndef COBRA_TEST

cobra::future_task<void> run(cobra::executor* exec) {
	using namespace std::chrono_literals;
	auto time = 20ms;
	auto t1 = exec->schedule(sleep(time));
	auto t2 = exec->schedule(sleep(time));
	co_await t1;
	co_await t2;
}

int main() {
	cobra::thread_pool_executor exec;
	auto task = run(&exec);
	task.get_future().wait();
	return EXIT_SUCCESS;
}
#endif
