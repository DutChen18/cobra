#ifndef COBRA_ASYNCIO_TASK_HH
#define COBRA_ASYNCIO_TASK_HH

#include "cobra/asyncio/coroutine.hh"
#include "cobra/asyncio/result.hh"

namespace cobra {
	template <class T> class task_promise;

	template <class T> class task : public coroutine<task_promise<T>> {
	public:
		bool await_ready() const noexcept { return false; }

		auto await_suspend(std::coroutine_handle<> handle) const noexcept {
			coroutine<task_promise<T>>::handle().promise().set_next(handle);
			return coroutine<task_promise<T>>::handle();
		}

		T await_resume() { return coroutine<task_promise<T>>::handle().promise().result().get_value_move(); }
	};

	class task_final_suspend {
	public:
		bool await_ready() const noexcept { return false; }

		template <class T> auto await_suspend(std::coroutine_handle<task_promise<T>> handle) const noexcept {
			return handle.promise().next();
		}

		void await_resume() const noexcept { return; }
	};

	template <class T> class task_promise_base {
	protected:
		result<T> _result;

	public:
		void unhandled_exception() noexcept { _result.set_exception(std::current_exception()); }

		result<T>& result() noexcept { return _result; }
	};

	template <class T> class task_promise_impl : public task_promise_base<T> {
	public:
		void return_value(T value) noexcept { task_promise_base<T>::_result.set_value(std::move(value)); }
	};

	template <> class task_promise_impl<void> : public task_promise_base<void> {
	public:
		void return_void() noexcept { _result.set_value(); }
	};

	template <class T> class task_promise : public task_promise_impl<T> {
		std::coroutine_handle<> _next;

	public:
		task<T> get_return_object() noexcept { return {*this}; }

		auto initial_suspend() const noexcept { return std::suspend_always(); }

		auto final_suspend() const noexcept { return task_final_suspend(); }

		void set_next(std::coroutine_handle<> handle) noexcept { _next = handle; }

		std::coroutine_handle<> next() const noexcept { return _next; }
	};
} // namespace cobra

#endif
