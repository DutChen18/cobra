#include "cobra/asyncio/mutex.hh"

#include "cobra/asyncio/task.hh"

namespace cobra {
	void async_mutex::lock_event::operator()(event_handle<void>& handle) {
		bool locked;

		{
			std::lock_guard lock(_mutex->_mutex);
			locked = std::exchange(_mutex->_locked, true);

			if (locked) {
				_mutex->_queue.push(&handle);
			}
		}

		if (!locked) {
			handle.set_value();
		}
	}

	async_mutex::async_mutex(executor* exec) : _exec(exec) {}

	event<void, async_mutex::lock_event> async_mutex::lock() {
		return {{this}};
	}

	bool async_mutex::try_lock() {
		std::lock_guard lock(_mutex);
		return !std::exchange(_locked, true);
	}

	void async_mutex::unlock() {
		event_handle<void>* next = nullptr;

		{
			std::lock_guard lock(_mutex);

			if (_queue.empty()) {
				_locked = false;
			} else {
				next = _queue.front();
				_queue.pop();
			}
		}

		if (next) {
			_exec->schedule([next]() {
				next->set_value();
			});
		}
	}

	async_lock::async_lock(async_mutex& mutex) : _mutex(&mutex) {}

	async_lock::async_lock(async_lock&& other) : _mutex(std::exchange(other._mutex, nullptr)) {}

	async_lock::~async_lock() {
		if (_mutex) {
			_mutex->unlock();
		}
	}

	async_lock& async_lock::operator=(async_lock other) {
		std::swap(_mutex, other._mutex);
		return *this;
	}

	async_mutex* async_lock::mutex() const {
		return _mutex;
	}

	task<async_lock> async_lock::lock(async_mutex& mutex) {
		co_await mutex.lock();
		co_return mutex;
	}

	void async_condition_variable::wait_event::operator()(event_handle<void>& handle) {
		{
			std::lock_guard lock(_condition_variable->_mutex);
			_condition_variable->_queue.push({&handle, _lock});
		}

		_lock->mutex()->unlock();
	}

	async_condition_variable::async_condition_variable(executor* exec) : _exec(exec) {}

	void async_condition_variable::notify_one() {
		std::pair<event_handle<void>*, async_lock*> next = {nullptr, nullptr};

		{
			std::lock_guard lock(_mutex);

			if (!_queue.empty()) {
				next = _queue.front();
				_queue.pop();
			}
		}

		if (next.first) {
			(void)_exec->schedule([](event_handle<void>* handle, async_lock* lock) -> task<void> {
				co_await lock->mutex()->lock();
				handle->set_value();
			}(next.first, next.second));
		}
	}

	void async_condition_variable::notify_all() {
		std::size_t size;

		{
			std::lock_guard lock(_mutex);
			size = _queue.size();
		}

		for (std::size_t i = 0; i < size; i++) {
			notify_one();
		}
	}

	event<void, async_condition_variable::wait_event> async_condition_variable::wait(async_lock& lock) {
		return {{this, &lock}};
	}
} // namespace cobra
