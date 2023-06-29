#ifndef COBRA_ASYNCIO_STREAM_HH
#define COBRA_ASYNCIO_STREAM_HH

#include "cobra/asyncio/task.hh"

#include <concepts>
#include <optional>
#include <string>
#include <vector>

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
	};

	template <class T, class CharT, class SizeT>//Not tested
	concept AsyncWriteable = requires(T t, const CharT* data, SizeT size) {
		{ t.read(data, size) } -> std::convertible_to<task<SizeT>>;
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
	class basic_istream : public basic_stream<CharT, Traits> {
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
	class basic_ostream : public basic_stream<CharT, Traits> {
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

	template<class CharT, class Traits = std::char_traits<CharT>>
	class basic_buffered_istream : public basic_istream<CharT, Traits> {
	public:
		using typename basic_istream<CharT, Traits>::char_type;
		using typename basic_istream<CharT, Traits>::traits_type;
		using typename basic_istream<CharT, Traits>::int_type;
		using typename basic_istream<CharT, Traits>::pos_type;
		using typename basic_istream<CharT, Traits>::off_type;

		[[nodiscard]] virtual task<std::optional<char_type>> peek() = 0;
	};

	template<AsyncReadableStream Stream>
	class basic_istream_reference : public basic_istream<typename Stream::char_type, typename Stream::traits_type> {
		Stream& _stream;

	public:
		basic_istream_reference(Stream& stream) : _stream(stream) {}
		basic_istream_reference(basic_istream_reference& other) : _stream(other._stream) {}
		basic_istream_reference(basic_istream_reference&& other) : _stream(other._stream) {}

		using char_type = typename Stream::char_type;
		using traits_type = typename Stream::traits_type;
		using int_type = typename Stream::int_type;
		using pos_type = typename Stream::pos_type;
		using off_type = typename Stream::off_type;

		task<std::size_t> read(char_type* data, std::size_t size) override {
			return _stream.read(data, size);
		}

		task<std::optional<char_type>> peek() requires AsyncPeekableStream<Stream> {
			return _stream.peek();
		}
	};

	template<AsyncReadableStream Stream>
	basic_istream_reference<Stream> wrap_stream(Stream& stream) {
		return basic_istream_reference<Stream>(stream);
	}

	template<class CharT, class Traits = std::char_traits<CharT>>
	class basic_buffered_ostream : public basic_ostream<CharT, Traits> {
	public:
		using typename basic_ostream<CharT, Traits>::char_type;
		using typename basic_ostream<CharT, Traits>::traits_type;
		using typename basic_ostream<CharT, Traits>::int_type;
		using typename basic_ostream<CharT, Traits>::pos_type;
		using typename basic_ostream<CharT, Traits>::off_type;
	};

	template <class Stream>
	class basic_istream_buffer : public basic_buffered_istream<typename Stream::char_type, typename Stream::traits_type> {
		Stream _stream;
		std::vector<typename Stream::char_type> _buffer;
		std::size_t _buffer_end = 0;
		std::size_t _buffer_begin = 0;

	public:
		using char_type = typename Stream::char_type;
		using traits_type = typename Stream::traits_type;
		using int_type = typename Stream::int_type;
		using pos_type = typename Stream::pos_type;
		using off_type = typename Stream::off_type;

		basic_istream_buffer(std::size_t buffer_size) {
			_buffer.resize(buffer_size);
		}

		task<std::size_t> read(char_type* data, std::size_t size) override {
			auto count = std::min(co_await fill(), size);
			auto begin = std::advance(_buffer.begin(), _buffer_begin);
			auto end = std::advance(_buffer.begin(), _buffer_begin += count);
			std::copy(begin, end, data);
			co_return count;
		}

		task<std::optional<char_type>> peek() {
			if (co_await fill() > 0) {
				co_return _buffer[_buffer_begin];
			} else {
				co_return std::nullopt;
			}
		}

	private:
		task<std::size_t> fill() {
			if (_buffer_end >= _buffer_begin) {
				_buffer_end = co_await _stream.read(_buffer.size());
				_buffer_begin = 0;
			}

			co_return _buffer_end - _buffer_begin;
		}
	};

	template <class Stream>
	class basic_ostream_buffer : public basic_buffered_ostream<typename Stream::char_type, typename Stream::traits_type> {
		Stream _stream;

	public:
		using char_type = typename Stream::char_type;
		using traits_type = typename Stream::traits_type;
		using int_type = typename Stream::int_type;
		using pos_type = typename Stream::pos_type;
		using off_type = typename Stream::off_type;

		task<std::size_t> write(const char_type* data, std::size_t size) override {
			return _stream.write(data, size);
		}

		task<void> flush() override {
			return _stream.flush();
		}
	};

	using stream = basic_stream<char>;
	using istream = basic_istream<char>;
	using ostream = basic_ostream<char>;
	using buffered_istream = basic_buffered_istream<char>;
	using buffered_ostream = basic_buffered_ostream<char>;

	//static_assert(AsyncReadable<buffered_istream, char, std::size_t>);
	static_assert(AsyncPeekableStream<buffered_istream>);
} // namespace cobra

#endif
