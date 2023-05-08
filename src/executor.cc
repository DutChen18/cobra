#include "cobra/executor.hh"
#include "cobra/runner.hh"

namespace cobra {
	executor::executor(runner& run) {
		this->run = &run;
	}

	executor::~executor() {
	}

	sequential_executor::sequential_executor(runner& run) : executor(run) {
	}

	void sequential_executor::exec(function<void>&& func) {
		func();
	}

	bool sequential_executor::done() const {
		return true;
	}

	thread_pool_executor::thread_pool_executor(runner& run, std::size_t size) : executor(run) {
		stopped = false;
		count = 0;

		for (std::size_t i = 0; i < size; i++) {
			threads.push_back(std::thread([this]() {
				std::unique_lock<std::mutex> guard(mutex);

				while (!stopped) {
					if (!funcs.empty()) {
						function<void> func(std::move(funcs.front()));

						funcs.pop();
						guard.unlock();
						func();
						guard.lock();

						std::lock_guard<std::mutex> guard(this->run->get_mutex());

						count -= 1;
						this->run->get_condition_variable().notify_all();

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
	
	void thread_pool_executor::exec(function<void>&& func) {
		std::unique_lock<std::mutex> guard(mutex);

		funcs.push(std::move(func));
		count += 1;
		condition_variable.notify_one();
	}

	bool thread_pool_executor::done() const {
		return count == 0;
	}
}
