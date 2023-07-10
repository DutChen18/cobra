#ifndef COBRA_ASYNCIO_STREAM_BUFFER_HH
#define COBRA_ASYNCIO_STREAM_BUFFER_HH

#include "cobra/asyncio/stream.hh"

#include <memory>

namespace cobra {
	template<class Stream>
	class istream_buffer : public basic_buffered_istream_impl<istream_buffer<Stream>, typename Stream::char_type, typename Stream::traits_type> {
		using base = basic_buffered_istream_impl<istream_buffer<Stream>, typename Stream::char_type, typename Stream::traits_type>;
		using char_type = typename Stream::char_type;

		Stream _stream;
		std::unique_ptr<char_type[]> _buffer;
		std::size_t _buffer_size;
		std::size_t _buffer_begin = 0;
		std::size_t _buffer_end = 0;

	public:
		istream_buffer(Stream&& stream, std::size_t buffer_size) : _stream(std::move(stream)) {
			_buffer = std::make_unique<char_type[]>(buffer_size);
			_buffer_size = buffer_size;
		}

		task<std::pair<const char_type*, std::size_t>> fill_buf() {
			if (_buffer_begin >= _buffer_end) {
				_buffer_begin = 0;
				_buffer_end = co_await _stream.read(_buffer.get(), _buffer_size);
			}

			co_return { _buffer.get() + _buffer_begin, _buffer_end - _buffer_begin };
		}

		void consume(std::size_t size) {
			_buffer_begin += size;
		}

		task<std::size_t> read(char_type* data, std::size_t size) {
			if (_buffer_begin >= _buffer_end && size >= _buffer_size) {
				co_return co_await _stream.read(data, size);
			}

			co_return co_await base::read(data, size);
		}

		Stream& inner() {
			return _stream;
		}
	};

	template<class Stream>
	class ostream_buffer : public basic_buffered_ostream_impl<ostream_buffer<Stream>, typename Stream::char_type, typename Stream::traits_type> {
		using char_type = typename Stream::char_type;

		Stream _stream;
		std::unique_ptr<char_type[]> _buffer;
		std::size_t _buffer_size;
		std::size_t _buffer_end = 0;

		task<void> flush_buf() {
			if (_buffer_end > 0) {
				co_await _stream.write_all(_buffer.get(), _buffer_end);
				_buffer_end = 0;
			}
		}

	public:
		ostream_buffer(Stream&& stream, std::size_t buffer_size) : _stream(std::move(stream)) {
			_buffer = std::make_unique<char_type[]>(buffer_size);
			_buffer_size = buffer_size;
		}

		task<std::size_t> write(const char_type* data, std::size_t size) {
			if (_buffer_end == 0 && size >= _buffer_size) {
				co_return co_await _stream.write(data, size);
			}

			auto count = std::min(size, _buffer_size - _buffer_end);
			std::copy(data, data + count, _buffer.get());
			_buffer_end += count;

			if (_buffer_end >= _buffer_size) {
				co_await flush_buf();
			}

			co_return count;
		}

		task<void> flush() {
			co_await flush_buf();
			co_await _stream.flush();
		}

		Stream& inner() {
			return _stream;
		}
	};
}

#endif
