#ifndef COBRA_ASYNCIO_FUTURE_HH
#define COBRA_ASYNCIO_FUTURE_HH

#include "cobra/asyncio/result.hh"

#include <coroutine>

namespace cobra {
	template<class T>
	class future {
	private:
		std::coroutine_handle<> _next;
		result<T> _result;

	public:
		bool await_ready() const {
			return _result.has_value();
		}

		void await_suspend(std::coroutine_handle<> caller) {
			_next = caller;
		}

		T await_resume() {
			return std::move(_result.value());
		}

		void set_value(T value) {
			_result.set_value(std::move(value));

			if (_next) {
				_next.resume();
			}
		}

		void set_exception(std::exception_ptr exception) {
			_result.set_exception(exception);

			if (_next) {
				_next.resume();
			}
		}
	};

	template<>
	class future<void> {
	private:
		std::coroutine_handle<> _next;
		result<void> _result;

	public:
		bool await_ready() const {
			return _result.has_value();
		}

		void await_suspend(std::coroutine_handle<> caller) {
			_next = caller;
		}

		void await_resume() const {
			_result.value();
		}

		void set_value() {
			_result.set_value();

			if (_next) {
				_next.resume();
			}
		}

		void set_exception(std::exception_ptr exception) {
			_result.set_exception(exception);

			if (_next) {
				_next.resume();
			}
		}
	};
}

#endif
