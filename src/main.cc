#include "cobra/event_loop.hh"
#include "cobra/executor.hh"
#include "cobra/future.hh"
#include "cobra/socket.hh"

#include <iostream>
#include <memory>
#include <thread>
#include <chrono>

/*
cobra::future<> on_connect(cobra::iosocket&& sock) {
	std::shared_ptr<cobra::iosocket> socket = std::make_shared<cobra::iosocket>(std::move(sock));
	std::cout << "connection" << std::endl;
	
	return cobra::async_while([socket]() {
		std::shared_ptr<std::vector<unsigned char>> buffer = std::make_shared<std::vector<unsigned char>>(1024);

		return socket->read_some(buffer->data(), buffer->size()).then<bool>([socket, buffer](std::size_t count) {
			return socket->write(buffer->data(), count).then<bool>([buffer, count](std::size_t) {
				return count != 0;
			});
		});
	});
}

int main() {
	cobra::server srv("localhost", "25565", on_connect);
	cobra::runner run;
	cobra::thread_pool_executor exec(run, 8);
	cobra::epoll_event_loop loop(run);
	cobra::context<> ctx(&exec, &loop);

	srv.start().run(ctx);
	run.run(&exec, &loop);
}
*/

cobra::future<int> fib(int i) {
	if (i < 2) {
		return 1;
	} else {
		return cobra::all_flat(
			fib(i - 1),
			fib(i - 2)
		).then<int>([](int j, int k) {
			return j + k;
		});
	}
}

int main() {
	cobra::runner run;
	cobra::sequential_executor exec(run);

	for (int i = 0; i < 20; i++) {
		cobra::context<int> ctx(&exec, nullptr);

		fib(10).run(std::move(ctx));
		run.run(&exec, nullptr);
	}
}
