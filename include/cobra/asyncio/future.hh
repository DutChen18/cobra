#ifndef COBRA_FUTURE_HH
#define COBRA_FUTURE_HH

#include "cobra/asyncio/result.hh"

#include <coroutine>

namespace cobra {
	template<class T>
	class future_base {
	protected:
		std::coroutine_handle<> _next;
		result<T> _result;

	public:
		bool await_ready() const {
			return _result.has_value();
		}

		void await_suspend(std::coroutine_handle<> caller) {
			_next = caller;
		}

		T await_resume() const {
			return _result.value();
		}
	};

	template<class T>
	class future : public future_base<T> {
	public:
		void set_value(T value) {
			future_base<T>::_result.set_value(value);

			if (future_base<T>::_next) {
				future_base<T>::_next.resume();
			}
		}

		void set_exception(std::exception_ptr exception) {
			future_base<T>::_result.set_exception(exception);

			if (future_base<T>::_next) {
				future_base<T>::_next.resume();
			}
		}
	};

	template<>
	class future<void> : public future_base<void> {
	public:
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
