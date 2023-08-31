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

	bool sequential_executor::has_jobs() {
		return false;
	}

	thread_pool_executor::thread_pool_executor() {
		create_threads(std::thread::hardware_concurrency());
	}

	thread_pool_executor::thread_pool_executor(std::size_t count) {
		create_threads(count);
	}

	thread_pool_executor::~thread_pool_executor() {
		{
			std::lock_guard lock(_mutex);
			_stop = true;
			_condition_variable.notify_all();
		}

		for (std::thread& thread : _threads) {
			thread.join();
		}

		_threads.clear();
	}

	void thread_pool_executor::schedule(std::function<void()> func) {
		std::lock_guard lock(_mutex);
		_queue.emplace(func);
		_jobs.fetch_add(1);
		_condition_variable.notify_one();
	}

	bool thread_pool_executor::has_jobs() {
		return _jobs.load() > 0;
	}

	void thread_pool_executor::create_threads(std::size_t count) {
		for (std::size_t i = 0; i < count; i++) {
			_threads.emplace_back([this]() {
				std::unique_lock lock(_mutex);

				while (!_stop) {
					if (!_queue.empty()) {
						auto func = _queue.front();
						_queue.pop();
						lock.unlock();
						func();
						lock.lock();
						_jobs.fetch_sub(1);
						continue;
					}

					_condition_variable.wait(lock);
				}
			});
		}
	}

	sequential_executor global_executor;
} // namespace cobra
