#include "cobra/asyncio/executor.hh"

#include <mutex>

namespace cobra {
	void executor::executor_event::operator()(event_handle<void>& handle) {
		_exec->schedule_event(handle);
	}

	executor::~executor() {}

	executor::event_type executor::schedule() {
		return {{ this }};
	}

	void sequential_executor::schedule_event(executor::event_type::handle_type& handle) {
		handle.set_value();
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

	void thread_pool_executor::schedule_event(event_type::handle_type& handle) {
		std::lock_guard lock(_mutex);
		_queue.emplace(&handle);
		_condition_variable.notify_one();
	}

	void thread_pool_executor::create_threads(std::size_t count) {
		for (std::size_t i = 0; i < count; i++) {
			_threads.emplace_back([this](std::stop_token stop_token) {
				std::unique_lock lock(_mutex);

				while (!stop_token.stop_requested()) {
					if (!_queue.empty()) {
						event_type::handle_type& handle = *_queue.front();
						_queue.pop();
						lock.unlock();
						handle.set_value();
						lock.lock();
					}

					_condition_variable.wait(lock);
				}
			});
		}
	}
}
