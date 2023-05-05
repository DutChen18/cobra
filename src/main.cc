#include "cobra/event_loop.hh"
#include "cobra/executor.hh"
#include "cobra/future.hh"
#include "cobra/socket.hh"

#include <iostream>
#include <thread>
#include <chrono>

/*
cobra::future<> sleep(float time) {
	return cobra::future<>([time](const cobra::context<>& ctx) {
		std::this_thread::sleep_for(std::chrono::duration<float>(time));
		ctx.resolve();
	});
}

int main() {
	cobra::thread_pool_executor exec(10);
	
	cobra::all_flat(
		sleep(1),
		sleep(1)
	).start(cobra::context<>(&exec, nullptr, []() {}));
}*/

cobra::future<> on_connect(cobra::iosocket&&) {
	std::cout << "connection" << std::endl;
	return cobra::future<>();
}

int main() {
	cobra::server srv("localhost", "25565", on_connect);
	cobra::epoll_event_loop loop;

	cobra::sequential_executor exec;
	cobra::all_flat(
		srv.start()
	).start(cobra::context<>(&exec, &loop, []() {}));
	loop.run();
}
