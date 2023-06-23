#ifndef COBRA_ASYNCIO_GENERATOR_HH
#define COBRA_ASYNCIO_GENERATOR_HH

#include "cobra/asyncio/coroutine.hh"
#include "cobra/asyncio/result.hh"

#include <ranges>

namespace cobra {
	template<class T>
	class generator_promise;

	template<class T>
	class generator_iterator;

	template<class T>
	class generator : public coroutine<generator_promise<T>> {
	public:
		using promise_type = generator_promise<T>;

		explicit operator bool() const {
			return coroutine<generator_promise<T>>::_handle.promise().has_value();
		}

		const T& operator*() const {
			return coroutine<generator_promise<T>>::_handle.promise().value();
		}

		generator& operator++() {
			coroutine<generator_promise<T>>::_handle.promise().clear();
			coroutine<generator_promise<T>>::_handle.resume();
			return *this;
		}

		generator_iterator<T> begin() {
			return this;
		}

		generator_iterator<T> end() {
			return nullptr;
		}
	};

	template<class T>
	class generator_promise : public coroutine_promise, public result<T> {
	public:
		generator<T> get_return_object() {
			return { std::coroutine_handle<generator_promise>::from_promise(*this) };
		}

		void return_void() {
		}

		std::suspend_always yield_value(T value) {
			result<T>::set_value(std::move(value));
			return {};
		}

		void unhandled_exception() {
			result<T>::set_exception(std::current_exception());
		}
	};

	template<class T>
	class generator_iterator {
		generator<T>* _gen;
	
	public:
		using difference_type = std::ptrdiff_t;
		using value_type = T;
		using pointer = const T*;
		using reference = const T&;
		using iterator_category = std::input_iterator_tag;

		generator_iterator(generator<T>* gen = nullptr) : _gen(gen) {
		}

		const T& operator*() const {
			return **_gen;
		}

		generator_iterator& operator++() {
			++*_gen;
			return *this;
		}

		generator_iterator operator++(int) {
			return nullptr;
		}

		bool operator==(const generator_iterator& other) const {
			bool empty = _gen == nullptr || !static_cast<bool>(*_gen);
			bool other_empty = other._gen == nullptr || !static_cast<bool>(*other._gen);
			return _gen == other._gen || (empty && other_empty);
		}

		bool operator!=(const generator_iterator& other) const {
			return !(*this == other);
		}
	};

	static_assert(std::ranges::range<generator<int>>);
}

#endif
