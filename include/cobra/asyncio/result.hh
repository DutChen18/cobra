#ifndef COBRA_RESULT_HH
#define COBRA_RESULT_HH

#include <variant>
#include <exception>

namespace cobra {
	template<class T>
	class result {
		std::variant<std::exception_ptr, T> _result;

	public:
		void return_value(T value) {
			_result = value;
		}

		void unhandled_exception() {
			_result = std::current_exception();
		}

		T get() const {
			return std::get<T>(_result);
		}
	};

	template<>
	class result<void> {
		std::exception_ptr _result;

	public:
		void return_void() {
		}

		void unhandled_exception() {
			_result = std::current_exception();
		}

		void get() const {
		}
	};
}

#endif
