#ifndef COBRA_ASYNCIO_STREAM_HH
#define COBRA_ASYNCIO_STREAM_HH

#include "cobra/asyncio/task.hh"

#include <concepts>
#include <memory>
#include <optional>
#include <string>

namespace cobra {

	template <class T>
	concept Stream = requires(T t) {
		typename T::char_type;
		typename T::traits_type;
		typename T::int_type;
		typename T::pos_type;
		typename T::off_type;
	};

	template <class T, class CharT, class SizeT>
	concept AsyncReadable = requires(T& t, CharT* data, SizeT size) {
		{ t.read(data, size) } -> std::convertible_to<task<SizeT>>;
		{ t.get() } -> std::convertible_to<task<std::optional<CharT>>>;
	};

	template <class T, class CharT, class SizeT>
	concept AsyncWritable = requires(T& t, const CharT* data, SizeT size) {
		{ t.write(data, size) } -> std::convertible_to<task<SizeT>>;
		{ t.flush() } -> std::convertible_to<task<void>>;
		{ t.write_all(data, size) } -> std::convertible_to<task<SizeT>>;
	};

	template <class T, class CharT>
	concept AsyncPeekable = requires(T& t) {
		{ t.peek() } -> std::convertible_to<task<std::optional<CharT>>>;
	};

	template <class T>
	concept AsyncReadableStream = requires(T t) {
		requires Stream<T>;
		requires AsyncReadable<T, typename T::char_type, std::size_t>;
	};

	template <class T>
	concept AsyncWritableStream = requires(T t) {
		requires Stream<T>;
		requires AsyncWritable<T, typename T::char_type, std::size_t>;
	};

	template <class T>
	concept AsyncPeekableStream = requires(T t) {
		requires AsyncReadableStream<T>;
		requires AsyncPeekable<T, typename T::char_type>;
	};

	template <class CharT, class Traits = std::char_traits<CharT>>
	class basic_stream {
	public:
		using char_type = CharT;
		using traits_type = Traits;
		using int_type = typename Traits::int_type;
		using pos_type = typename Traits::pos_type;
		using off_type = typename Traits::off_type;

		virtual ~basic_stream() {}
	};

	template <class CharT, class Traits = std::char_traits<CharT>>
	class basic_istream : public virtual basic_stream<CharT, Traits> {
	public:
		using typename basic_stream<CharT, Traits>::char_type;
		using typename basic_stream<CharT, Traits>::traits_type;
		using typename basic_stream<CharT, Traits>::int_type;
		using typename basic_stream<CharT, Traits>::pos_type;
		using typename basic_stream<CharT, Traits>::off_type;

		[[nodiscard]] virtual task<std::size_t> read(char_type* data, std::size_t size) = 0;

		[[nodiscard]] virtual task<std::optional<char_type>> get() {
			char_type result;

			if (co_await read(&result, 1) == 1) {
				co_return result;
			} else {
				co_return std::nullopt;
			}
		}
	};

	template <class CharT, class Traits = std::char_traits<CharT>>
	class basic_ostream : public virtual basic_stream<CharT, Traits> {
	public:
		using typename basic_stream<CharT, Traits>::char_type;
		using typename basic_stream<CharT, Traits>::traits_type;
		using typename basic_stream<CharT, Traits>::int_type;
		using typename basic_stream<CharT, Traits>::pos_type;
		using typename basic_stream<CharT, Traits>::off_type;

		[[nodiscard]] virtual task<std::size_t> write(const char_type* data, std::size_t size) = 0;
		[[nodiscard]] virtual task<void> flush() = 0;

		[[nodiscard]] virtual task<std::size_t> write_all(const char_type* data, std::size_t size) {
			std::size_t index = 0;
			std::size_t ret = 1;

			while (index < size && ret > 0) {
				ret = co_await write(data + index, size - index);
				index += ret;
			}

			co_return index;
		}
	};

	template <class CharT, class Traits = std::char_traits<CharT>>
	class basic_buffered_istream : public basic_istream<CharT, Traits> {
	public:
		using typename basic_istream<CharT, Traits>::char_type;
		using typename basic_istream<CharT, Traits>::traits_type;
		using typename basic_istream<CharT, Traits>::int_type;
		using typename basic_istream<CharT, Traits>::pos_type;
		using typename basic_istream<CharT, Traits>::off_type;

		[[nodiscard]] virtual task<std::optional<char_type>> peek() = 0;
	};

	template <class CharT, class Traits = std::char_traits<CharT>>
	class basic_buffered_ostream : public basic_ostream<CharT, Traits> {
	public:
		using typename basic_ostream<CharT, Traits>::char_type;
		using typename basic_ostream<CharT, Traits>::traits_type;
		using typename basic_ostream<CharT, Traits>::int_type;
		using typename basic_ostream<CharT, Traits>::pos_type;
		using typename basic_ostream<CharT, Traits>::off_type;
	};

	template <AsyncReadableStream Stream>
	class istream_buffer : public basic_buffered_istream<typename Stream::char_type, typename Stream::traits_type> {
		Stream _stream;
		std::unique_ptr<typename Stream::char_type[]> _buffer;
		std::size_t _buffer_begin = 0;
		std::size_t _buffer_end = 0;
		std::size_t _buffer_size;

	public:
		using char_type = typename Stream::char_type;
		using traits_type = typename Stream::traits_type;
		using int_type = typename Stream::int_type;
		using pos_type = typename Stream::pos_type;
		using off_type = typename Stream::off_type;

		istream_buffer(Stream&& stream, std::size_t buffer_size) : _stream(std::move(stream)) {
			_buffer = std::make_unique<char_type[]>(buffer_size);
			_buffer_size = buffer_size;
		}

		[[nodiscard]] task<std::size_t> read(char_type* data, std::size_t size) override {
			if (_buffer_begin == _buffer_end) {
				if (size >= _buffer_size) {
					co_return co_await _stream.read(data, size);
				}

				_buffer_begin = 0;
				_buffer_end = co_await _stream.read(_buffer.get(), _buffer_size);
			}

			std::size_t count = std::min(_buffer_end - _buffer_begin, size);
			const char_type* begin = _buffer.get() + _buffer_begin;
			std::copy(begin, begin + count, data);
			_buffer_begin += count;
			co_return count;
		}

		[[nodiscard]] task<std::optional<char_type>> peek() {
			if (_buffer_begin == _buffer_end) {
				_buffer_begin = 0;
				_buffer_end = co_await _stream.read(_buffer.get(), _buffer_size);
			}

			if (_buffer_begin == _buffer_end) {
				co_return std::nullopt;
			} else {
				co_return _buffer[_buffer_begin];
			}
		}
	};

	template <AsyncWritableStream Stream>
	class ostream_buffer : public basic_buffered_ostream<typename Stream::char_type, typename Stream::traits_type> {
		Stream _stream;
		std::unique_ptr<typename Stream::char_type[]> _buffer;
		std::size_t _buffer_end = 0;
		std::size_t _buffer_size;

	public:
		using char_type = typename Stream::char_type;
		using traits_type = typename Stream::traits_type;
		using int_type = typename Stream::int_type;
		using pos_type = typename Stream::pos_type;
		using off_type = typename Stream::off_type;

		ostream_buffer(Stream&& stream, std::size_t buffer_size) : _stream(std::move(stream)) {
			_buffer = std::make_unique<char_type[]>(buffer_size);
			_buffer_size = buffer_size;
		}

		[[nodiscard]] task<std::size_t> write(const char_type* data, std::size_t size) override {
			if (size >= _buffer_size && _buffer_end == 0) {
				co_return co_await _stream.write(data, size);
			}

			std::size_t count = std::min(_buffer_size - _buffer_end, size);
			const char_type* begin = _buffer.get() + _buffer_end;
			std::copy(data, data + count, begin);
			_buffer_end += count;

			if (_buffer_end == _buffer_size) {
				co_await _stream.write_all(_buffer.get(), _buffer_end);
				_buffer_end = 0;
			}

			co_return count;
		}

		[[nodiscard]] task<void> flush() override {
			if (_buffer_end != 0) {
				co_await _stream.write_all(_buffer.get(), _buffer_end);
				_buffer_end = 0;
			}

			co_return co_await _stream.flush();
		}
	};

	template <Stream Stream>
	class stream_reference : public Stream::stream_type {
		std::reference_wrapper<Stream> _stream;

	public:
		stream_reference(Stream& stream) : _stream(stream) {}
		stream_reference(stream_reference& other) : _stream(other._stream) {}
		stream_reference(stream_reference&& other) : _stream(other._stream) {}

		using char_type = typename Stream::char_type;
		using traits_type = typename Stream::traits_type;
		using int_type = typename Stream::int_type;
		using pos_type = typename Stream::pos_type;
		using off_type = typename Stream::off_type;

		[[nodiscard]] task<std::size_t> read(char_type* data, std::size_t size)
			requires AsyncReadableStream<Stream>
		override {
			return _stream.read(data, size);
		}

		[[nodiscard]] task<std::size_t> write(const char_type* data, std::size_t size)
			requires AsyncWritableStream<Stream>
		override {
			return _stream.write(data, size);
		}

		[[nodiscard]] task<task<std::optional<char_type>>> peek()
			requires AsyncPeekableStream<Stream>
		override {
			return _stream.peek();
		}
	};

	template <AsyncReadableStream Stream>
	stream_reference<Stream> wrap_stream(Stream& stream) {
		return stream_reference<Stream>(stream);
	}

	using stream = basic_stream<char>;
	using istream = basic_istream<char>;
	using ostream = basic_ostream<char>;
	using buffered_istream = basic_buffered_istream<char>;
	using buffered_ostream = basic_buffered_ostream<char>;

	// static_assert(AsyncReadable<buffered_istream, char, std::size_t>);
	static_assert(AsyncPeekableStream<buffered_istream>);
} // namespace cobra

#endif
