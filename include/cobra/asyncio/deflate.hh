#ifndef COBRA_ASYNCIO_DEFLATE_HH
#define COBRA_ASYNCIO_DEFLATE_HH

#include "cobra/asyncio/stream.hh"
#include "cobra/asyncio/stream_buffer.hh"
#include "cobra/serde.hh"
#include "cobra/print.hh"

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
