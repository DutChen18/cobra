#ifndef COBRA_RINGBUFFER_HH
#define COBRA_RINGBUFFER_HH

#include <cassert>
#include <cstddef>
#include <iterator>
#include <limits>
#include <memory>
#include <ratio>
#include <utility>

namespace cobra {
	template <class T, class Allocator = std::allocator<T>>
	class ringbuffer;

	template <class T, class Pointer, class Reference>
	class ringbuffer_iterator {
	public:
		using iterator_category = std::random_access_iterator_tag;
		using value_type = T;
		using difference_type = std::ptrdiff_t;
		using pointer = Pointer;
		using reference = Reference;
		using size_type = typename ringbuffer<value_type>::size_type;

	private:
		ringbuffer<value_type>* _buffer = nullptr;
		size_type _index = 0;

	public:
		constexpr ringbuffer_iterator() noexcept {}
		constexpr ringbuffer_iterator(const ringbuffer<value_type>* buffer, size_type index)
			: _buffer(const_cast<ringbuffer<value_type>*>(buffer)), _index(index) {}
		constexpr ringbuffer_iterator(const ringbuffer_iterator& other) noexcept = default;

		constexpr ringbuffer_iterator& operator=(const ringbuffer_iterator& other) noexcept = default;

		constexpr bool operator==(const ringbuffer_iterator& other) const noexcept {
			return _buffer == other._buffer && pos() == other.pos();
		}

		constexpr bool operator!=(const ringbuffer_iterator& other) const noexcept = default;

		constexpr reference operator*() const noexcept {
			return _buffer->data()[_index % _buffer->actual_capacity()];
		}
		constexpr pointer operator->() const noexcept {
			return &(this->operator*());
		}

		constexpr ringbuffer_iterator& operator++() noexcept {
			++_index;
			return *this;
		}

		constexpr ringbuffer_iterator operator++(int) noexcept {
			ringbuffer_iterator tmp(*this);
			++_index;
			return tmp;
		}

		constexpr ringbuffer_iterator& operator--() noexcept {
			if (_index == 0) {
				_index = _buffer->capacity();
			} else {
				--_index;
			}
			return *this;
		}

		constexpr ringbuffer_iterator operator--(int) noexcept {
			ringbuffer_iterator tmp(*this);
			this->operator--();
			return tmp;
		}

		constexpr ringbuffer_iterator operator+(difference_type n) const noexcept {
			return ringbuffer_iterator(_buffer, _index + n);
		}

		constexpr ringbuffer_iterator& operator+=(difference_type n) noexcept {
			_index += n;
			return *this;
		}

		constexpr ringbuffer_iterator operator-(difference_type n) const noexcept {
			if (n > 0 && static_cast<size_type>(n) > _index) {
				return ringbuffer_iterator(_buffer, _buffer->actual_capacity() - n + _index);
			}
			return ringbuffer_iterator(_buffer, (_index - n) % _buffer->actual_capacity());
		}

		constexpr difference_type operator-(const ringbuffer_iterator& other) const noexcept {
			const size_type actual_cap = _buffer->actual_capacity();
			const size_type a = pos();
			const size_type b = other.pos();
			const size_type begin = _buffer->buffer_begin();

			if (b < begin && a >= begin) {
				return -static_cast<difference_type>(actual_cap) + static_cast<difference_type>(a) -
					   static_cast<difference_type>(b);
			} else if (a < begin && b >= begin) {
				return static_cast<difference_type>(actual_cap) - static_cast<difference_type>(b) +
					   static_cast<difference_type>(a);
			}
			return static_cast<difference_type>(a) - static_cast<difference_type>(b);
		}

		constexpr ringbuffer_iterator& operator-=(difference_type n) const noexcept {
			_index = (_index - n) % _buffer->capacity();
			return *this;
		}

		constexpr bool operator<(const ringbuffer_iterator& other) const noexcept {
			return (*this - other) < 0;
		}

		constexpr bool operator>(const ringbuffer_iterator& other) const noexcept {
			return other < *this;
		}

		constexpr bool operator>=(const ringbuffer_iterator& other) const noexcept {
			return !(*this < other);
		}

		constexpr bool operator<=(const ringbuffer_iterator& other) const noexcept {
			return !(*this > other);
		}

		constexpr reference operator[](size_type n) const noexcept {
			return _buffer->data()[(_index + n) % _buffer->actual_capacity];
		}

		constexpr void swap(ringbuffer_iterator& other) noexcept {
			std::swap(_buffer, other._buffer);
			std::swap(_index, other._index);
		}

	private:
		constexpr size_type pos() const noexcept {
			return _index % _buffer->actual_capacity();
		}
	};

	template <class T, class Allocator>
	class ringbuffer {
	public:
		using value_type = T;
		using allocator_type = Allocator;
		using size_type = std::size_t;
		using difference_type = std::ptrdiff_t;
		using reference = value_type&;
		using const_reference = const value_type&;
		using pointer = value_type*;
		using const_pointer = const value_type*;
		using iterator = ringbuffer_iterator<value_type, pointer, reference>;
		using const_iterator = ringbuffer_iterator<value_type, const_pointer, const_reference>;
		using reverse_iterator = std::reverse_iterator<iterator>;
		using const_reverse_iterator = std::reverse_iterator<const_iterator>;

	private:
		size_type _buffer_capacity;
		size_type _buffer_begin = 0;
		size_type _buffer_size = 0;
		allocator_type _allocator;
		value_type* _buffer;

	public:
		// ODOT exception safety
		constexpr ringbuffer(std::size_t buffer_capacity)
			: _buffer_capacity(buffer_capacity), _allocator(), _buffer(_allocator.allocate(_buffer_capacity + 1)) {
			if (_buffer_capacity == std::numeric_limits<size_type>::max())
				throw std::logic_error("ringbuffer too large");
		}

		constexpr ringbuffer(const ringbuffer& other)
			: _buffer_capacity(other._buffer_capacity), _buffer_begin(other._buffer_begin),
			  _buffer_size(other._buffer_size), _allocator(), _buffer(_allocator.allocate(_buffer_capacity + 1)) {
			// ODOT exception safety
			std::uninitialized_copy(other._buffer, other._buffer + _buffer_size, _buffer);
		}

		constexpr ringbuffer(ringbuffer&& other) noexcept
			: _buffer_capacity(std::exchange(other._buffer_capacity, 0)),
			  _buffer_begin(std::exchange(other._buffer_begin, 0)), _buffer_size(std::exchange(other._buffer_size, 0)),
			  _allocator(std::move(other._allocator)), _buffer(std::exchange(other._buffer, nullptr)) {}

		constexpr ~ringbuffer() {
			if (_buffer != nullptr) {
				clear();
				_allocator.deallocate(data(), actual_capacity());
			}
		}

		constexpr iterator begin() noexcept {
			return iterator(this, _buffer_begin);
		}

		constexpr iterator end() noexcept {
			return iterator(this, (_buffer_begin + _buffer_size) % actual_capacity());
		}

		constexpr const_iterator begin() const noexcept {
			return const_iterator(this, _buffer_begin);
		}

		constexpr const_iterator end() const noexcept {
			return const_iterator(this, (_buffer_begin + _buffer_size) % actual_capacity());
		}

		constexpr reverse_iterator rbegin() noexcept {
			return reverse_iterator(end());
		}

		constexpr reverse_iterator rend() noexcept {
			return reverse_iterator(begin());
		}

		constexpr const_reverse_iterator rbegin() const noexcept {
			return const_reverse_iterator(end());
		}

		constexpr const_reverse_iterator rend() const noexcept {
			return const_reverse_iterator(begin());
		}

		constexpr size_type buffer_begin() const noexcept {
			return _buffer_begin;
		}
		constexpr size_type size() const noexcept {
			return _buffer_size;
		}
		constexpr size_type capacity() const noexcept {
			return _buffer_capacity;
		}
		constexpr size_type actual_capacity() const noexcept {
			return capacity() + 1;
		}
		constexpr bool empty() const noexcept {
			return size() == 0;
		}
		constexpr size_type remaining() const noexcept {
			return capacity() - size();
		}
		constexpr bool full() const noexcept {
			return remaining() == 0;
		}
		constexpr pointer data() noexcept {
			return _buffer;
		}
		constexpr const_pointer data() const noexcept {
			return _buffer;
		}
		constexpr size_type max_size() const noexcept {
			return std::numeric_limits<size_type>::max() + 1;
		}

		constexpr const_reference operator[](size_t n) const noexcept {
			return _buffer[(_buffer_begin + n) % actual_capacity()];
		}

		constexpr reference operator[](size_t n) noexcept {
			return const_cast<reference>(static_cast<const ringbuffer*>(this)->operator[](n));
		}

		constexpr void clear() noexcept {
			for (auto& v : *this) {
				std::destroy_at(&v);
			}
			_buffer_size = 0;
			_buffer_begin = 0;
		}

		constexpr void swap(ringbuffer& other) noexcept {
			std::swap(_buffer_capacity, other._buffer_capacity);
			std::swap(_buffer_begin, other._buffer_begin);
			std::swap(_buffer_size, other._buffer_size);
			std::swap(_allocator, other._allocator);
			std::swap(_buffer, other._buffer);
		}

		constexpr const value_type& front() const noexcept {
			assert(!empty() && "tried to get front of empty ringbuffer");
			return *begin();
		}

		constexpr const value_type& back() const noexcept {
			assert(!empty() && "tried to get back of empty ringbuffer");
			return *--end();
		}

		constexpr value_type& front() noexcept {
			return const_cast<value_type&>(static_cast<const ringbuffer*>(this)->front());
		}

		constexpr value_type& back() noexcept {
			return const_cast<value_type&>(static_cast<const ringbuffer*>(this)->back());
		}

		constexpr iterator push_back(const value_type& value) noexcept {
			size_type pos = (_buffer_begin + size()) % actual_capacity();
			if (full()) {
				pop_front();
			}
			std::construct_at(_buffer + pos, value);
			++_buffer_size;
			return --end();
		}

		constexpr iterator push_back(value_type&& value) noexcept {
			size_type pos = (_buffer_begin + size()) % actual_capacity();
			if (full()) {
				pop_front();
			}
			std::construct_at(_buffer + pos, std::move(value));
			++_buffer_size;
			return --end();
		}

		constexpr value_type pop_front() noexcept {
			assert(!empty() && "pop_front on empty ringbuffer");

			value_type result = std::move(front());
			// value_type result = back();

			_buffer_begin = (_buffer_begin + 1) % actual_capacity();
			--_buffer_size;

			return std::move(result);
		}

		// ODOT exception safety
		template <class InputIt>
		constexpr iterator insert(InputIt first, InputIt last) noexcept {
			iterator res = begin();
			bool inserted = false;

			for (; first != last; ++first) {
				// ODOT optimize
				iterator it = push_back(*first);
				if (!inserted) {
					res = it;
					inserted = true;
				}
			}
			return res;
		}

		constexpr void erase_front(size_type count) noexcept {
			assert(count <= size() && "tried to erase more than available");

			// ODOT optimize
			while (count--) {
				pop_front();
			}
		}
	};

	template <class T, class Alloc>
	inline constexpr void swap(ringbuffer<T, Alloc>& lhs, ringbuffer<T, Alloc>& rhs) noexcept {
		lhs.swap(rhs);
	}
} // namespace cobra
#endif
