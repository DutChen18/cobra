#ifndef COBRA_ASYNCIO_DEFLATE_HH
#define COBRA_ASYNCIO_DEFLATE_HH

#include "cobra/asyncio/stream.hh"
#include "cobra/asyncio/stream_buffer.hh"
#include "cobra/serde.hh"
#include "cobra/print.hh"

#include <cassert>
#include <algorithm>
#include <concepts>
#include <cstddef>
#include <functional>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <numeric>

#define COBRA_DEFLATE_NONE 0
#define COBRA_DEFLATE_FIXED 1
#define COBRA_DEFLATE_DYNAMIC 2

namespace cobra {
	enum class inflate_error {
		not_finished,
		short_buffer,
		long_distance,
		bad_block_type,
		bad_len_check,
		bad_size_code,
		bad_dist_code,
		bad_huffman_code,
		bad_trees,
		tree_too_stupid,
	};

	// TODO: peek_bits?
	template <AsyncInputStream Stream>
	class bit_istream {
		Stream _stream;
		std::size_t _count = 0;
		std::uint8_t _data;

	public:
		bit_istream(Stream&& stream) : _stream(std::move(stream)) {
		}

		task<std::uintmax_t> read_bits(std::size_t size) {
			std::uintmax_t value = 0;
			std::size_t index = 0;

			while (index < size) {
				std::size_t offset = _count % 8;

				if (offset == 0) {
					_data = co_await read_u8(_stream);
				}

				std::size_t count = std::min(8 - offset, size - index);
				value |= ((_data >> offset) & ((1 << count) - 1)) << index;
				index += count;
				_count += count;
			}

			co_return value;
		}

		Stream end()&& {
			return std::move(_stream);
		}
	};

	template <AsyncOutputStream Stream>
	class bit_ostream {
		Stream _stream;
		std::size_t _count = 0;
		std::uint8_t _data = 0;

	public:
		bit_ostream(Stream&& stream) : _stream(std::move(stream)) {
		}

		task<void> write_bits(std::uintmax_t value, std::size_t size) {
			while (size > 0) {
				std::size_t offset = _count % 8;
				std::size_t count = std::min(8 - offset, size);
				_data |= (value & ((1 << count) - 1)) << offset;
				value >>= count;
				size -= count;
				_count += count;

				if (_count % 8 == 0) {
					co_await write_u8(_stream, _data);
					_data = 0;
				}
			}
		}

		task<Stream> end()&& {
			if (_count % 8 != 0) {
				co_await write_u8(_stream, _data);
			}

			co_return std::move(_stream);
		}
	};

	template <class Base, class CharT, class Traits = std::char_traits<CharT>>
	class basic_istream_ringbuffer : public basic_buffered_istream_impl<basic_istream_ringbuffer<Base, CharT, Traits>, CharT, Traits> {
	public:
		using typename basic_buffered_istream_impl<basic_istream_ringbuffer<Base, CharT, Traits>, CharT, Traits>::char_type;
	
	private:
		std::unique_ptr<char_type[]> _buffer;
		std::size_t _buffer_size;
		std::size_t _buffer_begin = 0;
		std::size_t _buffer_end = 0;

		std::pair<std::size_t, std::size_t> space(std::size_t from, std::size_t to) const {
			std::size_t begin = from % _buffer_size;
			return { begin, std::min(_buffer_size - begin, to - from) };
		}

	protected:
		template <class Stream>
		task<std::size_t> write(Stream& stream, std::size_t size) {
			auto [begin, limit] = space(_buffer_end, _buffer_begin + _buffer_size);
			limit = std::min(limit, size);
			limit = std::min(limit, co_await stream.read(_buffer.get() + begin, limit));

			if (limit == 0 && size != 0) {
				throw stream_error::incomplete_read;
			}

			_buffer_end += limit;
			co_return limit;
		}

		std::size_t write(const char_type* data, std::size_t size) {
			auto [begin, limit] = space(_buffer_end, _buffer_begin + _buffer_size);
			limit = std::min(limit, size);
			std::copy(data, data + limit, _buffer.get() + begin);
			_buffer_end += limit;
			return limit;
		}

		std::size_t copy(std::size_t dist, std::size_t size) {
			if (dist > _buffer_size) {
				throw inflate_error::short_buffer;
			}

			if (dist > _buffer_end) {
				throw inflate_error::long_distance;
			}

			std::size_t first = (_buffer_end - dist) % _buffer_size;
			auto [begin, limit] = space(_buffer_end, _buffer_begin + _buffer_size);
			limit = std::min(limit, std::min(size, _buffer_size - first));
			
			char_type* buffer_out = _buffer.get() + begin;
			char_type* buffer_in = _buffer.get() + first;

			for (std::size_t i = 0; i < limit; i++) {
				buffer_out[i] = buffer_in[i];
			}

			_buffer_end += limit;

			return limit;
		}

		bool empty() const {
			return _buffer_begin >= _buffer_end;
		}

		bool full() const {
			return _buffer_end >= _buffer_begin + _buffer_size;
		}

	public:
		basic_istream_ringbuffer(std::size_t buffer_size) {
			_buffer = std::make_unique<char_type[]>(buffer_size);
			_buffer_size = buffer_size;
		}
		
		task<std::pair<const char_type*, std::size_t>> fill_buf() {
			co_await static_cast<Base*>(this)->fill_ringbuf();
			auto [begin, limit] = space(_buffer_begin, _buffer_end);
			co_return { _buffer.get() + begin, limit };
		}

		void consume(std::size_t size) {
			_buffer_begin += size;
		}
	};

	template <class Base>
	using istream_ringbuffer = basic_istream_ringbuffer<Base, char>;

	template <class T, std::size_t Size, std::size_t Bits>
	class inflate_tree {
		std::array<T, Size> _data;
		std::array<T, Bits + 1> _count;

	public:
		// TODO: sanitize
		inflate_tree(const std::size_t* size, std::size_t count) {
			std::array<T, Bits + 1> next;

			std::fill(_count.begin(), _count.end(), 0);

			for (T i = 0; i < count; i++) {
				if (size[i] != 0) {
					_count[size[i]] += 1;
				}
			}

			std::partial_sum(_count.begin(), std::prev(_count.end()), std::next(next.begin()));

			for (T i = 0; i < count; i++) {
				if (size[i] != 0) {
					if (next[size[i]] >= Size) {
						throw inflate_error::tree_too_stupid;
					}

					_data[next[size[i]]++] = i;
				}
			}
		}

		template <AsyncInputStream Stream>
task<T> read(bit_istream<Stream>& stream) const {
			T offset = 0;
			T value = 0;

			for (std::size_t i = 0; i <= Bits; i++) {
				if (value < _count[i]) {
					co_return _data[value + offset];
				}

				offset += _count[i];
				value -= _count[i];
				value <<= 1;
				value |= co_await stream.read_bits(1);
			}

			throw inflate_error::bad_huffman_code;
		}
	};

	using inflate_ltree = inflate_tree<std::uint16_t, 288, 15>;
	using inflate_dtree = inflate_tree<std::uint8_t, 30, 15>;
	using inflate_ctree = inflate_tree<std::uint8_t, 19, 7>;

	constexpr std::array<std::size_t, 19> frobnication_table {
		16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15,
	};

	constexpr std::array<std::size_t, 288> inflate_fixed_tree {
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
		9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
		9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
		9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
		9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
		9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
		9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
		9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
		7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
		7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8,
	};

	template <AsyncInputStream Stream>
	class inflate_istream : public istream_ringbuffer<inflate_istream<Stream>> {
		using base = istream_ringbuffer<inflate_istream<Stream>>;

		struct state_init {
			bit_istream<Stream> stream;
		};

		struct state_write {
			Stream stream;
			std::size_t limit;
		};

		struct state_read {
			bit_istream<Stream> stream;
			inflate_ltree lt;
			std::optional<inflate_dtree> dt;
			std::size_t dist;
			std::size_t size;
		};

		std::variant<state_init, state_write, state_read> _state;
		bool _final = false;

		static task<std::uint16_t> decode(bit_istream<Stream>& stream, std::uint16_t code, std::uint16_t stride) {
			std::uint16_t extra_bits = code / stride;
			std::uint16_t block_offset = (stride << extra_bits) - stride;
			std::uint16_t start_offset = (code % stride) << extra_bits;
			co_return start_offset + block_offset + co_await stream.read_bits(extra_bits);
		}

		static task<std::size_t> decode_code(bit_istream<Stream>& stream, std::uint8_t code) {
			if (code == 16) {
				co_return co_await stream.read_bits(2) + 3;
			} else if (code == 17) {
				co_return co_await stream.read_bits(3) + 3;
			} else if (code == 18) {
				co_return co_await stream.read_bits(7) + 11;
			} else {
				co_return 1;
			}
		}

		static task<std::uint16_t> decode_size(bit_istream<Stream>& stream, std::uint16_t code) {
			if (code >= 286) {
				throw inflate_error::bad_size_code;
			} else if (code == 285) {
				co_return 258;
			} else if (code < 261) {
				co_return code - 257 + 3;
			} else {
				co_return co_await decode(stream, code - 261, 4) + 7;
			}
		}

		static task<std::uint16_t> decode_dist(bit_istream<Stream>& stream, std::uint16_t code) {
			if (code >= 30) {
				throw inflate_error::bad_dist_code;
			} else if (code < 2) {
				co_return code + 1;
			} else {
				co_return co_await decode(stream, code - 2, 2) + 3;
			}
		}

	public:
		inflate_istream(Stream&& stream) : base(32768), _state(state_init { bit_istream(std::move(stream)) }) {
		}

		task<void> fill_ringbuf() {
			while (!base::full()) {
				if (auto* state = std::get_if<state_init>(&_state)) {
					if (_final) {
						co_return;
					}

					_final = co_await state->stream.read_bits(1) == 1;
					int type = co_await state->stream.read_bits(2);

					if (type == COBRA_DEFLATE_NONE) {
						Stream stream = std::move(state->stream).end();
						std::uint16_t len = co_await read_u16_le(stream);
						std::uint16_t nlen = co_await read_u16_le(stream);

						if (len != static_cast<std::uint16_t>(~nlen)) {
							throw inflate_error::bad_len_check;
						}

						_state = state_write { std::move(stream), len };
					} else if (type == COBRA_DEFLATE_FIXED) {
						inflate_ltree lt(inflate_fixed_tree.data(), inflate_fixed_tree.size());

						_state = state_read { std::move(state->stream), lt, std::nullopt, 0, 0 };
					} else if (type == COBRA_DEFLATE_DYNAMIC) {
						std::size_t hl = co_await state->stream.read_bits(5) + 257;
						std::size_t hd = co_await state->stream.read_bits(5) + 1;
						std::size_t hc = co_await state->stream.read_bits(4) + 4;

						std::array<std::size_t, 320> l;
						std::array<std::size_t, 19> lc;

						std::fill(l.begin(), l.end(), 0);
						std::fill(lc.begin(), lc.end(), 0);

						for (std::size_t i = 0; i < hc; i++) {
							lc[frobnication_table[i]] = co_await state->stream.read_bits(3);
						}

						inflate_ctree ct(lc.data(), lc.size());

						for (std::size_t i = 0; i < hl + hd;) {
							std::uint8_t v = co_await ct.read(state->stream);
							std::size_t n = co_await decode_code(state->stream, v);

							if (i + n > hl + hd) {
								throw inflate_error::bad_trees;
							} else if (v == 16 && i == 0) {
								throw inflate_error::bad_trees;
							} else if (v == 16) {
								v = l[i - 1];
							} else if (v == 17 || v == 18) {
								v = 0;
							}

							while (n-- > 0) {
								l[i++] = v;
							}
						}

						inflate_ltree lt(l.data(), hl);
						inflate_dtree dt(l.data() + hl, hd);

						_state = state_read { std::move(state->stream), lt, dt, 0, 0 };
					} else {
						throw inflate_error::bad_block_type;
					}
				} else if (auto* state = std::get_if<state_write>(&_state)) {
					state->limit -= co_await base::write(state->stream, state->limit);

					if (state->limit == 0) {
						_state = state_init { bit_istream(std::move(state->stream)) };
					}
				} else if (auto* state = std::get_if<state_read>(&_state)) {
					if (state->size > 0) {
						state->size -= base::copy(state->dist, state->size);
					} else {
						std::uint16_t code = co_await state->lt.read(state->stream);

						if (code < 256) {
							char c = std::char_traits<char>::to_char_type(code);
							base::write(&c, 1);
						} else if (code == 256) {
							_state = state_init { std::move(state->stream) };
						} else {
							state->size = co_await decode_size(state->stream, code);
							code = state->dt ? co_await state->dt->read(state->stream) : co_await state->stream.read_bits(5);
							state->dist = co_await decode_dist(state->stream, code);
						}
					}
				}
			}
		}

		Stream end()&& {
			if (_final) {
				if (auto* state = std::get_if<state_init>(&_state)) {
					return std::move(state->stream).end();
				}
			}

			throw inflate_error::not_finished;
		}
	};

	template <class UnsignedT, class CharT>
	class lz_basic_command {
		using char_type = CharT;
		using int_type = UnsignedT;

	public:
		int_type _length;
		union {
			int_type _distance;
			char_type _character;
		};

	public:
		constexpr lz_basic_command(char_type ch) noexcept : _length(1), _character(ch) {}
		constexpr lz_basic_command(int_type length, int_type dist) noexcept : _length(length), _distance(dist) {}
		constexpr lz_basic_command(const lz_basic_command& other) noexcept = default;

		constexpr lz_basic_command& operator=(const lz_basic_command& other) noexcept = default;
		constexpr bool operator==(const lz_basic_command& other) const noexcept = default;

		constexpr inline bool is_literal() const noexcept { return _length == 1; }
		constexpr inline int_type length() const noexcept { return _length; }
		constexpr inline int_type dist() const noexcept { return _distance; }
		constexpr inline char_type ch() const noexcept { return _character; }
	};

	using lz_command = lz_basic_command<unsigned short, unsigned char>;

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
		constexpr bool operator==(const ringbuffer_iterator& other) const noexcept = default;
		constexpr bool operator!=(const ringbuffer_iterator& other) const noexcept = default;

		constexpr reference operator*() const noexcept { return _buffer->data()[_index % _buffer->actual_capacity()]; }
		constexpr pointer operator->() const noexcept { return &(this->operator*()); }

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
			_index = (_index - 1) % _buffer->capacity();
			return *this;
		}

		constexpr ringbuffer_iterator operator--(int) noexcept {
			ringbuffer_iterator tmp(*this);
			_index = (_index - 1) % _buffer->capacity();
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
			return ringbuffer_iterator(_buffer, (_index - n) % _buffer->capacity());
		}

		constexpr difference_type operator-(const ringbuffer_iterator& other) const noexcept {
			const size_type actual_cap = _buffer->actual_capacity();
			const size_type a = pos();
			const size_type b = other.pos();
			const size_type begin = _buffer->buffer_begin();
			
			if (a < begin && b >= begin) {
				return static_cast<difference_type>(-a) - static_cast<difference_type>(actual_cap - b);
			} else if (b < begin && a >= begin) {
				return actual_cap - a + b;
			}
			return a - b;
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
		constexpr ringbuffer(std::size_t buffer_capacity) : _buffer_capacity(buffer_capacity), _allocator(), _buffer(_allocator.allocate(buffer_capacity)) {
			if (_buffer_capacity == 0)
				throw std::logic_error("ringbuffer too large");
			//_buffer = std::make_unique<value_type[]>(_buffer_capacity + 1);
		}

		constexpr ~ringbuffer() {
			clear();
			_allocator.deallocate(data(), actual_capacity());
		}

		constexpr iterator begin() noexcept {
			return iterator(this, _buffer_begin);
		}

		constexpr iterator end() noexcept {
			return iterator(this, _buffer_begin + _buffer_size % actual_capacity());
		}

		constexpr const_iterator begin() const noexcept {
			return const_iterator(this, _buffer_begin);
		}

		constexpr const_iterator end() const noexcept {
			return const_iterator(this, _buffer_begin + _buffer_size % actual_capacity());
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

		constexpr size_type buffer_begin() const noexcept { return _buffer_begin; }
		constexpr size_type size() const noexcept { return _buffer_size; }
		constexpr size_type capacity() const noexcept { return _buffer_capacity; }
		constexpr size_type actual_capacity() const noexcept { return capacity() + 1; }
		constexpr bool empty() const noexcept { return size() == 0; }
		constexpr size_type remaining() const noexcept { return capacity() - size() - 1; }
		constexpr bool full() const noexcept { return remaining() == 0; }
		constexpr pointer data() noexcept { return _buffer; }
		constexpr const_pointer data() const noexcept { return _buffer; }
		constexpr size_type max_size() const noexcept { return std::numeric_limits<size_type>::max() + 1; }

		constexpr void clear() noexcept {
			for (auto& v : *this) {
				std::destroy_at(&v);
			}
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

		constexpr void push_back(const value_type& value) noexcept {
			assert(size() + 1 < actual_capacity() && "push_back on full ringbuffer");

			std::construct_at(data() + size(), value);
			++_buffer_size;
		}

		constexpr value_type pop_front() noexcept {
			assert(!empty() && "pop_front on empty ringbuffer");

			value_type result = std::move(back());
			//value_type result = back();

			_buffer_begin = (_buffer_begin + 1) % actual_capacity();
			--_buffer_size;

			return std::move(result);
		}

		template <class InputIt>
		constexpr void insert(InputIt first, InputIt last) noexcept {
			for (; first != last; ++first) {
				//TODO optimize
				push_back(*first);
			}
		}

		constexpr void erase_front(size_type count) noexcept {
			assert(count < size() && "tried to erase more than available");

			//TODO optimize
			while (count--) {
				pop_front();
			}
		}
	};

	template <class T>
	class window {
	public:
		using value_type = typename ringbuffer<T>::value_type;
		using size_type = typename ringbuffer<T>::size_type;
		using difference_type = typename ringbuffer<T>::difference_type;
		using reference = typename ringbuffer<T>::reference;
		using pointer = typename ringbuffer<T>::pointer;
		using const_pointer = typename ringbuffer<T>::const_pointer;
		using iterator = typename ringbuffer<T>::iterator;
		using const_iterator = typename ringbuffer<T>::const_iterator;
		using reverse_iterator = typename ringbuffer<T>::reverse_iterator;
		using const_reverse_iterator = typename ringbuffer<T>::const_reverse_iterator;

	private:
		ringbuffer<T> _buffer;
	
	public:
		constexpr window(size_type buffer_size) : _buffer(buffer_size) {}

		constexpr iterator begin() noexcept { return _buffer.begin(); }
		constexpr iterator end() noexcept { return _buffer.end(); }
		constexpr const_iterator begin() const noexcept { return _buffer.begin(); }
		constexpr const_iterator end() const noexcept { return _buffer.end(); }
		constexpr reverse_iterator rbegin() noexcept { return _buffer.rbegin(); }
		constexpr reverse_iterator rend() noexcept { return _buffer.rend(); }
		constexpr const_reverse_iterator rbegin() const noexcept { return _buffer.rbegin(); }
		constexpr const_reverse_iterator rend() const noexcept { return _buffer.rend(); }

		constexpr size_type capacity() const noexcept { return _buffer.capacity(); }
		constexpr bool empty() const noexcept { return _buffer.empty(); }
		constexpr size_type remaining() const noexcept { return _buffer.remaining(); }
		constexpr bool full() const noexcept { return _buffer.full(); }
		constexpr pointer data() noexcept { return _buffer.data(); }
		constexpr const_pointer data() const noexcept { return _buffer.data(); }
		constexpr size_type max_size() const noexcept { return std::numeric_limits<size_type>::max() + 1; }

		constexpr const value_type& front() const noexcept {
			return _buffer.front();
		}

		constexpr const value_type& back() const noexcept {
			return _buffer.back();
		}

		constexpr value_type& front() noexcept {
			return _buffer.front();
		}

		constexpr value_type& back() noexcept {
			return _buffer.back();
		}

		constexpr void push_back(const value_type& value) noexcept {
			if (full()) {
				pop_front();
				push_back(value);
			} else {
				_buffer.push_back(value);
			}
		}

		constexpr value_type pop_front() noexcept {
			return _buffer.pop_front();
		}

		template <class InputIt>
		constexpr void insert(InputIt first, InputIt last) noexcept {
			for (; first != last; ++first) {
				//TODO optimize
				push_back(first);
			}
		}

		constexpr void erase_front(size_type count) noexcept {
			_buffer.erase_front(count);
		}
	};

	template <class UnsignedT, class CharT>
	class lz_istream {
		using char_type = CharT;
		using command_type = lz_basic_command<UnsignedT, CharT>; 
		window<char_type> _window;

	public:
		lz_istream(std::size_t buffer_size) : _window(buffer_size) {}

		task<void> write(const command_type& command) {
			if (command.is_literal()) {
				std::cout << command.ch();
				write_char(command.ch());
			} else {
				print("({},{})", command.length(), command.dist());
				auto it = _window.rbegin() + command.dist();
				auto end = it + command.length();

				for (; it != end; ++it) {
					write_char(*it);
				}
			}
			co_return;
		}

		void write_char(char_type ch) {
			//std::cout << ch;
			_window.push_back(ch);
		}
	};

	template <class Stream, class UnsignedT, class CharT>
	class lz_ostream : public ostream_impl<lz_ostream<Stream, UnsignedT, CharT>> {
	public:
		using base_type = ostream_impl<lz_ostream<Stream, UnsignedT, CharT>>;
		using typename base_type::char_type;
		using command_type = lz_basic_command<UnsignedT, CharT>; 

		using distance_type = UnsignedT;
		using length_type = UnsignedT;

	private:
		class chain {
		public:
			using iterator = chain*;
			using const_iterator = chain*;

		private:
			using buffer_iterator_type = typename ringbuffer<char_type>::iterator;
			using const_buffer_iterator_type = typename ringbuffer<char_type>::const_iterator;

			CharT _ch;
			chain* _next;
			buffer_iterator_type _pos;

		public:
			constexpr chain(const chain& other) noexcept = default;

			inline iterator begin() { return this; }
			inline const_iterator begin() const { return this; }

			inline iterator end() { return nullptr; }
			inline const_iterator end() const { return nullptr; }

			constexpr bool operator==(const chain& other) const noexcept = default;
			constexpr bool operator==(const CharT& ch) const noexcept {
				return _ch == ch;
			}

			inline buffer_iterator_type pos() { return _pos; };
			inline const_buffer_iterator_type pos() const { return _pos; };

			inline chain* next() { _next; };
			inline const chain* next() const { _pos; };
		};

		Stream _stream;
		std::size_t _table_size;
		ringbuffer<char_type> _buffer;
		ringbuffer<char_type> _window;//TODO use window type
		ringbuffer<chain> _table;

		static constexpr length_type max_length = std::numeric_limits<length_type>::max();
		static constexpr distance_type max_dist = std::numeric_limits<distance_type>::max();
		static constexpr length_type min_backref_length = 3;

	public:
		lz_ostream(Stream&& stream, std::size_t window_size)
			: _stream(std::move(stream)), _table_size(window_size), _buffer(_table_size), _window(_table_size),
			  _table(_table_size) {}

		task<std::size_t> write(const char_type* data, const std::size_t size) {
			if (_buffer.remaining() >= size) {
				_buffer.insert(data, data + size);
			} else if (size < _buffer.size()) {
				co_await flush_atleast(size);
				co_return co_await write(data, size);
			} else {
				for (std::size_t index = 0; index < size; index += _buffer.capacity()) {
					co_await flush();
					_buffer.insert(data, data + std::min(size - index, _buffer.capacity()));
				}
			}
			co_return size;
		}

		task<Stream> end() && {
			co_await flush();
			co_return std::move(_stream);
		}

		task<void> flush() {
			co_return co_await flush_atleast(_buffer.size());
		}

	private:
		task<void> flush_atleast(size_t at_least) {
			assert(at_least <= _buffer.size() && "tried to flush more than available");
			while (at_least > 0) {
				if (_buffer.size() < min_backref_length) {
					eprintln("buffer to small");
					co_await write_literal();
					--at_least;
					continue;
				}

				auto it = std::find(_table.begin(), _table.end(), *_buffer.begin());

				if (it == _table.end()) {
					eprintln("no backreferences found");
					//No backreferences available
					co_await write_literal();
					--at_least;
					continue;
				}

				chain* best = nullptr;
				length_type best_len = 0;

				for (chain& cur : *it) {
					auto [start, end]= std::mismatch(cur.pos(), _window.end(), _buffer.begin(), _buffer.end());
					auto len = end - start;
					if (len > min_backref_length && len > best_len) {
						best = &cur;
						best_len = len;
					}

					if (len > max_length) {
						break;
					}
				}

				if (best == nullptr) {
					eprintln("no backreferenced long enough found");
					co_await write_literal();
					--at_least;
					continue;
				}

				co_await write_backref(best_len, best->pos() - _buffer.begin());
				at_least -= best_len;
			}
		}
		task<length_type> write_literal() {
			CharT ch = _buffer.pop_front();

			if (_window.full())
				_window.pop_front();

			_window.push_back(ch);
			co_await _stream.write(command_type(ch));
			co_return 1;
		}

		task<length_type> write_backref(length_type len, distance_type dist) {
			auto beg = _buffer.begin();
			auto end = beg + len;

			_window.insert(beg, end);
			_buffer.erase_front(len);
			co_await _stream.write(command_type(len, dist));
			co_return len;
		}

		auto find(CharT ch) {
			return std::find(_table.begin(), _table.end(), ch);
		}

		//task<std::size_t> write(const
	};

	//TODO use umlaut (brötli)
	template <AsyncOutputStream Stream>
	class brotli_ostream : public ostream_impl<brotli_ostream<Stream>> {
	public:
		using typename ostream_impl<brotli_ostream<Stream>>::char_type;

	private:
		bit_ostream<Stream> _stream;
	
	public:
		brotli_ostream(Stream&& stream) : _stream(std::move(stream)) {}

		task<std::size_t> write(const char_type* data, std::size_t size) {
			if (size == 0) {
				co_await write_byte(6);
				co_return 0;
			}

			std::cout << "size: " << size << std::endl;
			std::size_t i = 0;
			co_await write_byte(12);
			for (i = 0; i + 65535 < size; i += 65536) {
				co_await write_byte(248);
				co_await write_byte(255);
				co_await write_byte(15);
				co_await write_bytes(&data[i], 65536);
			}
			if (i < size) {
				int r = size - i - 1;
				co_await write_byte((r & 31) << 3);
				co_await write_byte(r >> 5);
				co_await write_byte(8 + (r >> 13));
				co_await write_bytes(&data[i], r + 1);
			}
			co_await write_byte(3);
			co_return size;
		}

		task<void> flush() {
			auto inner = co_await std::move(_stream).end();
			co_await inner.flush();
			_stream = bit_ostream(std::move(inner));
		}

		task<Stream> end() && {
			//TODO send end to brotli stream
			return std::move(_stream).end();
		}

	private:
		task<void> write_byte(char byte) {
			co_await _stream.write_bits(byte, 8);
		}

		task<void> write_bytes(const char* bytes, std::size_t size) {
			for (std::size_t i = 0; i < size; ++i) {
				co_await write_byte(bytes[i]);
			}
		}
	};
}

#endif
