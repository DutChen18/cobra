#ifndef COBRA_ASYNCIO_EVENT_HH
#define COBRA_ASYNCIO_EVENT_HH

#include "cobra/asyncio/result.hh"

#include <coroutine>

namespace cobra {
	template <class T>
	class event_handle_base {
	protected:
		std::coroutine_handle<T> _next;
		result<T> _result;

	public:
		void set_next(std::coroutine_handle<> handle) noexcept {
			_next = handle;
		}

		void set_exception(std::exception_ptr exception) noexcept {
			_result.set_exception(exception);
			_next.resume();
		}

		T value() {
			return _result.get_value_move();
		}
	};

	template <class T>
	class event_handle : public event_handle_base<T> {
	public:
		void set_value(T value) {
			event_handle_base<T>::_result.set_value(std::move(value));
			event_handle_base<T>::_next.resume();
		}
	};

	template <>
	class event_handle<void> : public event_handle_base<void> {
	public:
		void set_value() noexcept {
			_result.set_value();
			_next.resume();
		}
	};

	template <class T, class Function>
	class event {
		event_handle<T> _handle;
		Function _function;

	public:
		using handle_type = event_handle<T>;

		event(Function&& function) : _function(std::move(function)) {}

		bool await_ready() const noexcept {
			return false;
		}

		void await_suspend(std::coroutine_handle<> handle) noexcept {
			_handle.set_next(handle);
			_function(_handle);
		}

		T await_resume() {
			return _handle.value();
		}
	};
} // namespace cobra

#endif
