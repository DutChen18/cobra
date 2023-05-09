#include "cobra/event_loop.hh"
#include "cobra/executor.hh"
#include "cobra/future.hh"
#include "cobra/socket.hh"

#include <iostream>
#include <memory>
#include <thread>
#include <chrono>

cobra::future<> on_connect(cobra::iosocket&& sock) {
	std::vector<unsigned char> buffer(1024);
	
	return cobra::async_while<cobra::unit>(cobra::capture([](cobra::iosocket& sock, std::vector<unsigned char>& buffer) {
		return sock.read_some(buffer.data(), buffer.size()).then<cobra::optional_unit>([&sock, &buffer](std::size_t count) {
			return sock.write(buffer.data(), count).map<cobra::optional_unit>([count](std::size_t) {
				if (count == 0) {
					return cobra::some<cobra::unit>();
				} else {
					return cobra::none<cobra::unit>();
				}
			});
		});
	}, std::move(sock), std::move(buffer))).ignore();
}

int main() {
	cobra::server srv("localhost", "25565", on_connect);
	cobra::runner run;
	cobra::thread_pool_executor exec(run, 8);
	cobra::epoll_event_loop loop(run);
	cobra::context<> ctx(&exec, &loop);

	srv.start().run(std::move(ctx));
	run.run(&exec, &loop);
}

/*
cobra::future<int> fib(int i) {
	if (i < 2) {
		return cobra::future<int>(1);
	} else {
		return cobra::all_flat(
			fib(i - 1),
			fib(i - 2)
		).map<int>([](int j, int k) {
			return j + k;
		});
	}
}

int main() {
	cobra::runner run;
	cobra::thread_pool_executor exec(run, 4);
	// cobra::sequential_executor exec(run);

	for (int i = 0; i < 10; i++) {
		cobra::context<int> ctx(&exec, nullptr, [](int result) {
			std::cout << result << std::endl;
		});

		fib(10).run(std::move(ctx));
		run.run(&exec, nullptr);
	}
}
*/
