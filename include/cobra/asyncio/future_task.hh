#ifndef COBRA_ASYNCIO_FUTURE_TASK_HH
#define COBRA_ASYNCIO_FUTURE_TASK_HH

#include "cobra/asyncio/coroutine.hh"

#include <future>

namespace cobra {
	template <class T>
	class future_task_promise;

	template <class T>
	class [[nodiscard]] future_task : public coroutine<future_task_promise<T>> {
	public:
		std::future<T> get_future() const noexcept {
			return coroutine<future_task_promise<T>>::handle().promise().get_future();
		}
	};

	template <class T>
	class future_task_promise_base {
	protected:
		std::promise<T> _promise;

	public:
		void unhandled_exception() noexcept {
			_promise.set_exception(std::current_exception());
		}

		std::future<T> get_future() noexcept {
			return _promise.get_future();
		}
	};

	template <class T>
	class future_task_promise_impl : public future_task_promise_base<T> {
	public:
		void return_value(T value) noexcept {
			future_task_promise_base<T>::_promise.set_value(std::move(value));
		}
	};

	template <>
	class future_task_promise_impl<void> : public future_task_promise_base<void> {
	public:
		void return_void() noexcept {
			_promise.set_value();
		}
	};

	template <class T>
	class future_task_promise : public future_task_promise_impl<T> {
	public:
		future_task<T> get_return_object() noexcept {
			return {*this};
		}

		auto initial_suspend() const noexcept {
			return std::suspend_never();
		}

		auto final_suspend() const noexcept {
			return std::suspend_always();
		}
	};

	template <class Awaitable>
	auto make_future_task(Awaitable awaitable) -> future_task<decltype(awaitable.await_resume())> {
		co_return co_await awaitable;
	}

	template <class Awaitable>
	auto block_task(Awaitable awaitable) -> decltype(awaitable.await_resume()) {
		return make_future_task(std::move(awaitable)).get_future().get();
	}
} // namespace cobra

#endif
