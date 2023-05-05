#include "cobra/executor.hh"

namespace cobra {
	executor::~executor() {
	}

	void sequential_executor::exec(std::function<void()> func) {
		func();
	}

	thread_pool_executor::thread_pool_executor(std::size_t size) {
		stopped = false;

		for (std::size_t i = 0; i < size; i++) {
			threads.push_back(std::thread([this]() {
				std::unique_lock<std::mutex> guard(mutex);

				while (!stopped) {
					if (!funcs.empty()) {
						std::function<void()> func = funcs.front();

						funcs.pop();
						guard.unlock();
						func();
						guard.lock();

						continue;
					}
					
					condition_variable.wait(guard);
				}
			}));
		}
	}

	thread_pool_executor::~thread_pool_executor() {
		{
			std::unique_lock<std::mutex> guard(mutex);

			stopped = true;
			condition_variable.notify_all();
		}

		for (std::thread& thread : threads) {
			thread.join();
		}
	}
	
	void thread_pool_executor::exec(std::function<void()> func) {
		std::unique_lock<std::mutex> guard(mutex);

		funcs.push(func);
	}
}
