#ifndef COBRA_ASYNCIO_RESULT_HH
#define COBRA_ASYNCIO_RESULT_HH

#include <exception>
#include <optional>

namespace cobra {
	class result_base {
		std::exception_ptr _exception;

	protected:
		void rethrow_exception() const {
			if (_exception) {
				std::rethrow_exception(_exception);
			}
		}

	public:
		std::exception_ptr exception() const noexcept {
			return _exception;
		}

		void set_exception(std::exception_ptr exception) noexcept {
			_exception = exception;
		}
	};

	template <class T>
	class result : public result_base {
		std::optional<T> _value;

		const T& value() const noexcept {
			return _value.value();
		}

		T value_move() noexcept {
			return std::move(_value.value());
		}

	public:
		void set_value(T value) noexcept {
			_value.emplace(std::move(value));
		}

		const T& get_value() const {
			rethrow_exception();
			return value();
		}

		T get_value_move() {
			rethrow_exception();
			return value_move();
		}

		bool has_value() const noexcept {
			return exception() || _value.has_value();
		}

		void reset() noexcept {
			_value.reset();
		}
	};

	template <>
	class result<void> : public result_base {
	public:
		void set_value() noexcept {
			return;
		}

		void get_value() const noexcept {
			rethrow_exception();
		}

		void get_value_move() noexcept {
			rethrow_exception();
		}
	};
}; // namespace cobra

#endif
