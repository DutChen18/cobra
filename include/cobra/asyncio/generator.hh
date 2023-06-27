#ifndef COBRA_ASYNCIO_GENERATOR_HH
#define COBRA_ASYNCIO_GENERATOR_HH

#include "cobra/asyncio/coroutine.hh"
#include "cobra/asyncio/result.hh"

#include <ranges>

namespace cobra {
	template <class T>
	class generator_promise;

	template <class T>
	class generator_iterator;

	template <class T>
	class generator : public coroutine<generator_promise<T>> {
	public:
		generator_iterator<T> begin() const noexcept {
			return this;
		}

		generator_iterator<T> end() const noexcept {
			return nullptr;
		}
	};

	template <class T>
	class generator_promise {
		result<T> _result;

	public:
		void unhandled_exception() noexcept {
			_result.set_exception(std::current_exception());
		}

		auto yield_value(T value) noexcept {
			_result.set_value(std::move(value));
			return std::suspend_always();
		}

		void return_void() noexcept {
			_result.reset();
		}

		const result<T>& result() const noexcept {
			return _result;
		}

		generator<T> get_return_object() noexcept {
			return {*this};
		}

		auto initial_suspend() const noexcept {
			return std::suspend_never();
		}

		auto final_suspend() const noexcept {
			return std::suspend_always();
		}

		template <class Task>
		void await_transform(Task task) const noexcept = delete;
	};

	template <class T>
	class generator_iterator {
		const generator<T>* _generator;

	public:
		using difference_type = std::ptrdiff_t;
		using value_type = T;
		using pointer = const T*;
		using reference = const T&;
		using iterator_category = std::input_iterator_tag;

		generator_iterator(const generator<T>* generator = nullptr) noexcept {
			_generator = generator;
		}

		const T& operator*() const {
			return _generator->handle().promise().result().get_value();
		}

		generator_iterator& operator++() noexcept {
			_generator->handle().resume();
			return *this;
		}

		generator_iterator operator++(int) noexcept {
			_generator->handle().resume();
			return nullptr;
		}

		bool operator==(const generator_iterator& other) const noexcept {
			bool empty = !_generator || !_generator->handle().promise().result().has_value();
			bool other_empty = !other._generator || !other._generator->handle().promise().result().has_value();
			return _generator == other._generator || (empty && other_empty);
		}

		bool operator!=(const generator_iterator& other) const noexcept {
			return !(*this == other);
		}
	};

	static_assert(std::ranges::range<generator<int>>);
} // namespace cobra

#endif
