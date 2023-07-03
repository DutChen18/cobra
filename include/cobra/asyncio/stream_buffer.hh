#ifndef COBRA_ASYNCIO_STREAM_BUFFER_HH
#define COBRA_ASYNCIO_STREAM_BUFFER_HH

#include "cobra/asyncio/stream.hh"

#include <memory>

namespace cobra {
	template<class Stream>
	class istream_buffer : public basic_buffered_istream_impl<istream_buffer<Stream>, typename Stream::char_type, typename Stream::traits_type> {
		using base = basic_buffered_istream_impl<istream_buffer<Stream>, typename Stream::char_type, typename Stream::traits_type>;
		using char_type = Stream::char_type;

		Stream _stream;
		std::unique_ptr<char_type[]> _buffer;
		std::size_t _buffer_size;
		std::size_t _buffer_begin = 0;
		std::size_t _buffer_end = 0;

	public:
		istream_buffer(Stream&& stream, std::size_t buffer_size) : _stream(stream) {
			_buffer = std::make_unique<char_type[]>(buffer_size);
			_buffer_size = buffer_size;
		}

		task<std::ranges::subrange<const char_type*>> fill_buf() {
			if (_buffer_begin >= _buffer_end) {
				_buffer_begin = 0;
				_buffer_end = co_await _stream.read(_buffer, _buffer_size);
			}

			co_return { _buffer + _buffer_begin, _buffer_end - _buffer_begin };
		}

		void consume(std::size_t size) {
			_buffer_begin += size;
		}

		task<std::size_t> read(char_type* data, std::size_t size) {
			if (_buffer_begin >= _buffer_end && size >= _buffer_size) {
				co_return _stream.read(data, size);
			}

			co_return co_await base::read(data, size);
		}
	};

	template<class Stream>
	class ostream_buffer : public basic_buffered_ostream_impl<ostream_buffer<Stream>, typename Stream::char_type, typename Stream::traits_type> {
		using char_type = Stream::char_type;

		Stream _stream;
		std::unique_ptr<char_type[]> _buffer;
		std::size_t _buffer_size;
		std::size_t _buffer_end = 0;

	public:
		ostream_buffer(Stream&& stream, std::size_t buffer_size) : _stream(stream) {
			_buffer = std::make_unique<char_type[]>(buffer_size);
			_buffer_size = buffer_size;
		}

		task<std::size_t> write(const char_type* data, std::size_t size) {
			
		}
	};
}

#endif
