#ifndef COBRA_ASYNCIO_STREAM_BUFFER_HH
#define COBRA_ASYNCIO_STREAM_BUFFER_HH

#include "cobra/asyncio/stream.hh"

#include <memory>

namespace cobra {
	template<AsyncInputStream Stream>
	class istream_buffer : public basic_buffered_istream_impl<istream_buffer<Stream>, typename Stream::char_type, typename Stream::traits_type> {
		using base = basic_buffered_istream_impl<istream_buffer<Stream>, typename Stream::char_type, typename Stream::traits_type>;

	public:
		using typename base::char_type;

	private:
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

	template<class Base, AsyncInputStream Stream>
	class istream_limit_base : public Base {
	protected:
		Stream _stream;
		std::size_t _limit;

	public:
		using char_type = typename Stream::char_type;

		istream_limit_base(Stream&& stream, std::size_t limit) : _stream(std::move(stream)) {
			_limit = limit;
		}

		task<std::size_t> read(char_type* data, std::size_t size) {
			size = std::min(size, _limit);
			std::size_t ret = co_await _stream.read(data, size);
			_limit -= ret;
			co_return ret;
		}

		Stream& inner() {
			return _stream;
		}

		std::size_t limit() const {
			return _limit;
		}
	};

	template<AsyncInputStream Stream>
	class istream_limit : public istream_limit_base<basic_istream_impl<istream_limit<Stream>, typename Stream::char_type, typename Stream::traits_type>, Stream> {
	public:
		istream_limit(Stream&& stream, std::size_t limit) : istream_limit_base<basic_istream_impl<istream_limit<Stream>, typename Stream::char_type, typename Stream::traits_type>, Stream>(std::move(stream), limit) {
		}
	};

	template<AsyncBufferedInputStream Stream>
	class istream_limit<Stream> : public istream_limit_base<basic_buffered_istream_impl<istream_limit<Stream>, typename Stream::char_type, typename Stream::traits_type>, Stream> {
	public:
		using base = istream_limit_base<basic_buffered_istream_impl<istream_limit<Stream>, typename Stream::char_type, typename Stream::traits_type>, Stream>;
		using typename base::char_type;

		istream_limit(Stream&& stream, std::size_t limit) : istream_limit_base<basic_buffered_istream_impl<istream_limit<Stream>, typename Stream::char_type, typename Stream::traits_type>, Stream>(std::move(stream), limit) {
		}

		task<std::pair<const char_type*, std::size_t>> fill_buf() {
			if (base::_limit > 0) {
				auto [buffer, size] = co_await base::_stream.fill_buf();
				co_return { buffer, std::min(size, base::_limit) };
			} else {
				co_return { nullptr, 0 };
			}
		}

		void consume(std::size_t size) {
			base::_limit -= size;
		}
	};

	template<AsyncOutputStream Stream>
	class ostream_buffer : public basic_buffered_ostream_impl<ostream_buffer<Stream>, typename Stream::char_type, typename Stream::traits_type> {
		using base = basic_buffered_ostream_impl<ostream_buffer<Stream>, typename Stream::char_type, typename Stream::traits_type>;

	public:
		using typename base::char_type;

	private:
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
			std::copy(data, data + count, _buffer.get() + _buffer_end);
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

	template<class Base, AsyncOutputStream Stream>
	class ostream_limit_base : public Base {
	protected:
		Stream _stream;
		std::size_t _limit;

	public:
		using char_type = typename Stream::char_type;

		ostream_limit_base(Stream&& stream, std::size_t limit) : _stream(std::move(stream)) {
			_limit = limit;
		}

		task<std::size_t> write(const char_type* data, std::size_t size) {
			size = std::min(size, _limit);
			std::size_t ret = co_await _stream.write(data, size);
			_limit -= ret;
			co_return ret;
		}

		task<void> flush() {
			return _stream.flush();
		}

		Stream& inner() {
			return _stream;
		}

		std::size_t limit() const {
			return _limit;
		}
	};

	template<AsyncOutputStream Stream>
	class ostream_limit : public ostream_limit_base<basic_ostream_impl<ostream_limit<Stream>, typename Stream::char_type, typename Stream::traits_type>, Stream> {
	public:
		ostream_limit(Stream&& stream, std::size_t limit) : ostream_limit_base<basic_ostream_impl<ostream_limit<Stream>, typename Stream::char_type, typename Stream::traits_type>, Stream>(std::move(stream), limit) {
		}
	};

	template<AsyncBufferedOutputStream Stream>
	class ostream_limit<Stream> : public ostream_limit_base<basic_buffered_ostream_impl<ostream_limit<Stream>, typename Stream::char_type, typename Stream::traits_type>, Stream> {
	public:
		ostream_limit(Stream&& stream, std::size_t limit) : ostream_limit_base<basic_buffered_ostream_impl<ostream_limit<Stream>, typename Stream::char_type, typename Stream::traits_type>, Stream>(std::move(stream), limit) {
		}
	};
}

#endif
