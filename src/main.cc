#include "cobra/event_loop.hh"
#include "cobra/executor.hh"
#include "cobra/future.hh"
#include "cobra/socket.hh"

#include <iostream>
#include <thread>
#include <chrono>

cobra::future<> on_connect(cobra::iosocket&& socket) {
	std::cout << "connection" << std::endl;
	
	return cobra::async_while([&socket]() {
		std::shared_ptr<std::vector<unsigned char>> buffer = std::make_shared<std::vector<unsigned char>>(1024);

		return socket.read_some(buffer->data(), buffer->size()).then<bool>([&socket, buffer](std::size_t count) {
			return socket.write(buffer->data(), count).then<bool>([count](std::size_t) {
				return count != 0;
			});
		});
	});
}

int main() {
	cobra::server srv("localhost", "25565", on_connect);
	cobra::epoll_event_loop loop;
	cobra::sequential_executor exec;
	cobra::context<> ctx(&exec, &loop);

	srv.start().run(ctx);
	loop.run();
}
