#ifndef COBRA_ASYNCIO_DEFLATE_HH
#define COBRA_ASYNCIO_DEFLATE_HH

#include "cobra/asyncio/stream.hh"
#include "cobra/asyncio/stream_buffer.hh"
#include "cobra/serde.hh"
#include "cobra/print.hh"

#include <vector>
#include <cstdint>
#include <memory>

#define COBRA_DEFLATE_NONE 0
#define COBRA_DEFLATE_FIXED 1
#define COBRA_DEFLATE_DYNAMIC 2

namespace cobra {
	enum class inflate_error {
		not_finished,
		bad_block_type,
		bad_len_check,
	};

	// TODO: peek
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
		std::size_t _buffer_begin;
		std::size_t _buffer_end;

		std::pair<std::size_t, std::size_t> space(std::size_t from, std::size_t to) const {
			std::size_t begin = from % _buffer_size;
			std::size_t limit = _buffer_size - begin;
			return { begin, std::min(limit, (limit + to) % _buffer_size) };
		}

	protected:
		template <class Stream>
		task<std::size_t> write(Stream& stream, std::size_t size) {
			auto [begin, limit] = space(_buffer_end, _buffer_begin);
			limit = std::min(limit, size);
			co_await stream.read_all(_buffer.get() + begin, limit);
			_buffer_end += limit;
			co_return limit;
		}

		std::size_t write(const char_type* data, std::size_t size) {
			auto [begin, limit] = space(_buffer_end, _buffer_begin);
			limit = std::min(limit, size);
			std::copy(data, data + limit, _buffer.get() + begin);
			_buffer_end += limit;
			co_return limit;
		}

		void copy(std::size_t distance, std::size_t length) {
			// TODO
		}

		bool empty() const {
			return _buffer_begin >= _buffer_end;
		}

	public:
		basic_istream_ringbuffer(std::size_t buffer_size) {
			_buffer = std::make_unique<char_type[]>(buffer_size);
			_buffer_size = buffer_size;
		}
		
		task<std::pair<const char_type*, std::size_t>> fill_buf() {
			if (_buffer_begin >= _buffer_end) {
				co_await static_cast<Base*>(this)->fill_ringbuf();
			}

			auto [begin, limit] = space(_buffer_begin, _buffer_end);
			co_return { _buffer.get() + begin, limit };
		}

		void consume(std::size_t size) {
			_buffer_begin += size;
		}
	};

	template <class Base>
	using istream_ringbuffer = basic_istream_ringbuffer<Base, char>;

	template <class T>
	class inflate_tree {
	public:
		template <AsyncInputStream Stream>
		task<T> read(bit_istream<Stream>& stream) const {
			co_return 0; // TODO
		}
	};

	using inflate_ltree = inflate_tree<std::uint16_t>;
	using inflate_dtree = inflate_tree<std::uint8_t>;

	template <AsyncInputStream Stream>
	class inflate_istream : public istream_ringbuffer<inflate_istream<Stream>> {
		using base = istream_ringbuffer<inflate_istream<Stream>>;

		enum class state {
			init,
			read,
			ref,
			copy,
		};

		state _state = state::init;
		bit_istream<Stream> _bit_stream;
		// istream_limit<Stream> _limit_stream;
		std::size_t _distance;
		std::size_t _length;
		bool _final = false;

		task<std::uint16_t> decode_length(std::uint16_t code) {

		}

		task<std::uint16_t> decode_distance(std::uint16_t code) {

		}

		task<void> inflate_none() {
			Stream stream = std::move(_bit_stream).end();
			std::uint16_t len = co_await read_u16_le(stream);
			std::uint16_t nlen = co_await read_u16_le(stream);

			if (len != static_cast<std::uint16_t>(~nlen)) {
				throw inflate_error::bad_len_check;
			}

			co_await base::write(stream, len);
			_bit_stream = std::move(stream);
		}

		task<void> inflate_fixed(const inflate_ltree* lt, const inflate_dtree* dt) {
			while (true) {
				std::uint16_t value = co_await lt->read(_bit_stream);

				if (value < 256) {
					char c = std::char_traits<char>::to_char_type(value);
					base::write(&c, 1);
				} else if (value == 256) {
					break;
				} else {
					std::uint16_t length = co_await decode_length(value);
					std::uint8_t code = dt ? co_await dt->read(_bit_stream) : co_await _bit_stream.read_bits(5);
					std::uint16_t distance = co_await decode_distance(code);
				}
			};
		}

	public:
		inflate_istream(Stream&& stream) : base(32768), _bit_stream(std::move(stream)) {
		}

		task<void> fill_ringbuf() {
			while (base::empty() && !_final) {
				_final = co_await _bit_stream.read_bits(1) == 1;
				int type = co_await _bit_stream.read_bits(2);

				switch (type) {
				case COBRA_DEFLATE_NONE:
					co_await inflate_none();
					break;
				case COBRA_DEFLATE_FIXED:
				case COBRA_DEFLATE_DYNAMIC:
				default:
					throw inflate_error::bad_block_type;
				}
			}
		}

		Stream end()&& {
			if (base::empty() || !_final) {
				throw inflate_error::not_finished;
			}

			return std::move(_bit_stream).end();
		}
	};
}

#endif
