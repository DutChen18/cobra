#ifndef COBRA_COMPRESS_STREAM_RINGBUFFER_HH
#define COBRA_COMPRESS_STREAM_RINGBUFFER_HH

#include "cobra/asyncio/stream.hh"
#include "cobra/compress/error.hh"

#include <memory>

namespace cobra {
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
				throw compress_error::short_buffer;
			}

			if (dist > _buffer_end) {
				throw compress_error::long_distance;
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
}

#endif
