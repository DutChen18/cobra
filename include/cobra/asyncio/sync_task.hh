#ifndef COBRA_ASYNCIO_SYNC_TASK_HH
#define COBRA_ASYNCIO_SYNC_TASK_HH

#include "cobra/asyncio/coroutine.hh"

#include <future>

namespace cobra {
	template <class T> class sync_task_promise;

	template <class T> class sync_task : public coroutine<sync_task_promise<T>> {
	public:
		std::future<T> get_future() const noexcept {
			return coroutine<sync_task_promise<T>>::handle().promise().get_future();
		}
	};

	template <class T> class sync_task_promise_base {
	protected:
		std::promise<T> _promise;

	public:
		void unhandled_exception() noexcept { _promise.set_exception(std::current_exception()); }

		std::future<T> get_future() noexcept { return _promise.get_future(); }
	};

	template <class T> class sync_task_promise_impl : public sync_task_promise_base<T> {
	public:
		void return_value(T value) noexcept { sync_task_promise_base<T>::_promise.set_value(std::move(value)); }
	};

	template <> class sync_task_promise_impl<void> : public sync_task_promise_base<void> {
	public:
		void return_void() noexcept { _promise.set_value(); }
	};

	template <class T> class sync_task_promise : public sync_task_promise_impl<T> {
	public:
		sync_task<T> get_return_object() noexcept { return {*this}; }

		auto initial_suspend() const noexcept { return std::suspend_never(); }

		auto final_suspend() const noexcept { return std::suspend_always(); }
	};
} // namespace cobra

#endif
