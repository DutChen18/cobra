#ifndef COBRA_TASK_HH
#define COBRA_TASK_HH

#include "cobra/asyncio/future.hh"

#include <coroutine>

namespace cobra {
	template<class T>
	class promise;

	template<class T>
	class task {
		std::coroutine_handle<promise<T>> _handle;

	public:
		using promise_type = promise<T>;

		task(std::coroutine_handle<promise<T>> handle) : _handle(handle) {
		}
		
		task(const task& other) = delete;

		task(task&& other) : _handle(std::move(other._handle)) {
		}

		~task() {
			_handle.destroy();
		}

		task& operator=(task other) {
			std::swap(_handle, other._handle);
			return *this;
		}

		promise<T>& operator co_await() const {
			return _handle.promise();
		}
	};
	
	template<class T>
	class promise_base : public future<T> {
	public:
		void return_value(T value) {
			future<T>::set_value(value);
		}

		void unhandled_exception() {
			future<T>::set_exception(std::current_exception());
		}
	};

	template<>
	class promise_base<void> : public future<void> {
	public:
		void return_void() {
			set_value();
		}

		void unhandled_exception() {
			set_exception(std::current_exception());
		}
	};

	template<class T>
	class promise : public promise_base<T> {
	public:
		task<T> get_return_object() {
			return { std::coroutine_handle<promise>::from_promise(*this) };
		}
		
		std::suspend_never initial_suspend() const noexcept {
			return {};
		}

		std::suspend_always final_suspend() const noexcept {
			return {};
		}
	};
}

#endif
