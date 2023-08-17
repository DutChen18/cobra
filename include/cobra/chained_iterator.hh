#ifndef COBRA_CHAINED_ITERATOR_HH
#define COBRA_CHAINED_ITERATOR_HH

#include <iterator>

namespace cobra {

	template <class Iter>
	class chained_iterator {
	public:
		using iterator_type = Iter;
		using traits_type = std::iterator_traits<iterator_type>;
		//using iterator_category = typename traits_type::iterator_category; //TODO use this
		using iterator_category = std::forward_iterator_tag;
		using value_type = typename traits_type::value_type;
		using pointer = typename traits_type::pointer;
		using reference = typename traits_type::reference;

	private:
		iterator_type _first_begin, _first_end;
		iterator_type _last_begin, _last_end;
		iterator_type _current;
		bool _last = false;
	
		constexpr chained_iterator(const chained_iterator& other, iterator_type current) noexcept : chained_iterator(other), _current(current) {}
	public:
		constexpr chained_iterator() noexcept : _first_begin(), _first_end(), _last_begin(), _last_end(), _current() {}
		constexpr chained_iterator(iterator_type first1, iterator_type last1, iterator_type first2,
								   iterator_type last2) noexcept
			: _first_begin(first1), _first_end(last1), _last_begin(first2), _last_end(last2) {
				if (_first_begin == _first_end) {
					_last = true;
					_current = _last_begin;
				} else {
					_current = _first_begin;
				}
		}

		constexpr chained_iterator begin() {
			return *this;
		}
		
		constexpr chained_iterator end() {
			return chained_iterator(*this, _last_end);
		}

		constexpr reference operator*() {
			return _current;
		}

		constexpr pointer operator->() {
			return _current.operator->();
		}

		constexpr chained_iterator& operator++() {
			if (_last) {
				++_current;
			} else if (++_current == _first_end) {
				_current = _last_begin;
				_last = true;
			}
			return *this;
		}

		constexpr chained_iterator operator++(int) {
			chained_iterator tmp(*this);
			this->operator++();
			return tmp;
		}

		constexpr void swap(chained_iterator& other) noexcept {
			std::swap(_first_begin, other._first_begin);
			std::swap(_first_end, other._first_end);
			std::swap(_last_begin, other._last_begin);
			std::swap(_last_end, other._last_end);
			std::swap(_last, other._last);
		}
	};
}

#endif
