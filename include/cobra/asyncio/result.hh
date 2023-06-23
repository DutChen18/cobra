#ifndef COBRA_ASYNCIO_RESULT_HH
#define COBRA_ASYNCIO_RESULT_HH

#include <variant>
#include <exception>

namespace cobra {
	template<class T>
	class result {
		std::variant<std::monostate, std::exception_ptr, T> _result;

	public:
		void clear() {
			_result.template emplace<std::monostate>();
		}

		void set_value(T value) {
			_result.template emplace<T>(std::move(value));
		}

		void set_exception(std::exception_ptr exception) {
			_result.template emplace<std::exception_ptr>(exception);
		}

		bool has_value() const {
			return !std::holds_alternative<std::monostate>(_result);
		}

		const T& value() const {
			if (auto exception = std::get_if<std::exception_ptr>(&_result)) {
				std::rethrow_exception(*exception);
			}

			return std::get<T>(_result);
		}

		T& value() {
			if (auto exception = std::get_if<std::exception_ptr>(&_result)) {
				std::rethrow_exception(*exception);
			}
			
			return std::get<T>(_result);
		}
	};

	template<>
	class result<void> {
		std::variant<std::monostate, std::exception_ptr> _result;

	public:
		void clear() {
			_result.emplace<std::monostate>();
		}

		void set_value() {
			_result.emplace<std::exception_ptr>(nullptr);
		}

		void set_exception(std::exception_ptr exception) {
			_result.emplace<std::exception_ptr>(exception);
		}

		bool has_value() const {
			return !std::holds_alternative<std::monostate>(_result);
		}

		void value() const {
			if (auto exception = std::get<std::exception_ptr>(_result)) {
				std::rethrow_exception(exception);
			}
		}
	};
}

#endif
