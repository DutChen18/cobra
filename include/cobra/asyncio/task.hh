#ifndef COBRA_ASYNCIO_TASK_HH
#define COBRA_ASYNCIO_TASK_HH

#include "cobra/asyncio/coroutine.hh"
#include "cobra/asyncio/future.hh"
#include <exception>

namespace cobra {
	template<class T>
	class task_promise;

	template<class T>
	class task : public coroutine<task_promise<T>> {
	public:
		using promise_type = task_promise<T>;

		task_promise<T>& operator co_await() const {
			return coroutine<task_promise<T>>::_handle.promise();
		}
	};

	template<class T>
	class task_promise : public coroutine_promise, public future<T> {
	public:
		task<T> get_return_object() {
			return { std::coroutine_handle<task_promise>::from_promise(*this) };
		}

		void return_value(T value) {
			future<T>::set_value(std::move(value));
		}

		void unhandled_exception() {
			std::rethrow_exception(std::current_exception());
			future<T>::set_exception(std::current_exception());
		}
	};

	template<>
	class task_promise<void> : public coroutine_promise, public future<void> {
	public:
		task<void> get_return_object() {
			return { std::coroutine_handle<task_promise>::from_promise(*this) };
		}

		void return_void() {
			set_value();
		}

		void unhandled_exception() {
			std::rethrow_exception(std::current_exception());
			set_exception(std::current_exception());
		}
	};
}

#endif
