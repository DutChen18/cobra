#ifndef COBRA_ASYNCIO_PROMISE_HH
#define COBRA_ASYNCIO_PROMISE_HH

#include "cobra/asyncio/result.hh"

#include <coroutine>

namespace cobra {
	template <class T>
	class promise_base {
	protected:
		result<T> _result;
		std::coroutine_handle<> _next;

	public:
		void unhandled_exception() noexcept {
			_result.set_exception(std::current_exception());
		}

		result<T>& result() noexcept {
			return _result;
		}

		void set_next(std::coroutine_handle<> handle) noexcept {
			_next = handle;
		}

		std::coroutine_handle<> next() const noexcept {
			return _next;
		}
	};

	template <class T>
	class promise : public promise_base<T> {
	public:
		void return_value(T value) noexcept {
			promise_base<T>::_result.set_value(std::move(value));
		}
	};

	template <>
	class promise<void> : public promise_base<void> {
	public:
		void return_void() noexcept {
			_result.set_value();
		}
	};
}

#endif
