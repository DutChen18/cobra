#include "cobra/asyncio/executor.hh"

#include <mutex>

namespace cobra {
	void executor::executor_event::operator()(event_handle<void>& handle) {
		_exec.get().schedule([&handle]() {
			handle.set_value();
		});
	}

	executor::~executor() {}

	executor::event_type executor::schedule() {
		return executor_event{*this};
	}

	void sequential_executor::schedule(std::function<void()> func) {
		func();
	}

	thread_pool_executor::thread_pool_executor() {
		create_threads(std::jthread::hardware_concurrency());
	}

	thread_pool_executor::thread_pool_executor(std::size_t count) {
		create_threads(count);
	}

	thread_pool_executor::~thread_pool_executor() {
		for (std::jthread& thread : _threads) {
			thread.request_stop();
		}

		{
			std::lock_guard lock(_mutex);
			_condition_variable.notify_all();
		}

		_threads.clear();
	}

	void thread_pool_executor::schedule(std::function<void()> func) {
		std::lock_guard lock(_mutex);
		_queue.emplace(func);
		_condition_variable.notify_one();
	}

	void thread_pool_executor::create_threads(std::size_t count) {
		for (std::size_t i = 0; i < count; i++) {
			_threads.emplace_back([this](std::stop_token stop_token) {
				std::unique_lock lock(_mutex);

				while (!stop_token.stop_requested()) {
					if (!_queue.empty()) {
						auto func = _queue.front();
						_queue.pop();
						lock.unlock();
						func();
						lock.lock();
						continue;
					}

					_condition_variable.wait(lock);
				}
			});
		}
	}
} // namespace cobra
