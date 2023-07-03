#ifndef COBRA_ASYNCIO_STREAM_UTILS_HH
#define COBRA_ASYNCIO_STREAM_UTILS_HH

#include "cobra/asyncio/stream.hh"

#include <optional>

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

	template <class T, class CharT, class SizeT>
	concept AsyncWritable = requires(T& t, const CharT* data, SizeT size) {
								{ t.write(data, size) } -> std::convertible_to<task<SizeT>>;
								{ t.flush() } -> std::convertible_to<task<void>>;
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

	template <AsyncReadableStream Stream>
	class take_stream : public basic_istream<typename Stream::char_type, typename Stream::traits_type> {
		Stream _stream;
		std::size_t _n;

	public:
		using char_type = typename Stream::char_type;
		using traits_type = typename Stream::traits_type;
		using int_type = typename Stream::int_type;
		using pos_type = typename Stream::pos_type;
		using off_type = typename Stream::off_type;

		constexpr take_stream(Stream stream, std::size_t count) noexcept : _stream(std::move(stream)), _n(count) {}

		task<std::size_t> read(char_type* data, std::size_t size) {
			std::size_t nread = co_await _stream.read(data, size < _n ? size : _n);
			_n -= nread;
			co_return nread;
		}

		task<std::optional<char_type>> get() {
			char_type result;
			if (co_await read(&result, 1) > 0) {
				co_return result;
			} else {
				co_return std::nullopt;
			}
		}

		task<std::optional<char_type>> peek()
			requires AsyncPeekableStream<Stream>
		{
			if (_n == 0) {
				return std::nullopt;
			} else {
				return _stream.peek();
			}
		}
	};

	template <AsyncPeekableStream PeekableStream, class UnaryPredicate>
	class take_while_stream
		: public basic_istream<typename PeekableStream::char_type, typename PeekableStream::traits_type> { //TODO make ideomatic
		PeekableStream _stream;
		UnaryPredicate _p;
		bool _flag;

	public:
		using char_type = typename PeekableStream::char_type;
		using traits_type = typename PeekableStream::traits_type;
		using int_type = typename PeekableStream::int_type;
		using pos_type = typename PeekableStream::pos_type;
		using off_type = typename PeekableStream::off_type;

		constexpr take_while_stream(PeekableStream stream, UnaryPredicate p) : _stream(std::move(stream)), _p(std::move(p)), _flag(false) {}

		task<std::size_t> read(char_type* data, std::size_t size) {
			std::size_t nwritten = 0;
			std::optional<char_type> ch;

			while (nwritten < size && (ch = co_await peek())) {
				data[nwritten] = *ch;
				++nwritten;
			}
			co_return nwritten;
		}

		task<std::optional<char_type>> get() {
			char_type result;
			if (co_await read(&result, 1) > 0) {
				co_return result;
			} else {
				co_return std::nullopt;
			}
		}

		task<std::optional<char_type>> peek() {
			if (!_flag) {
				auto ch = co_await _stream.peek();
				if (ch) {
					if (_p(*ch)) {
						co_return ch;
					}
				}
				_flag = true;
			}
			co_return std::nullopt;
		}
	};

	template <AsyncReadableStream Stream>
	class stream_adapter {
		using stream_type = Stream;

		stream_type _stream;

	public:
		using char_type = typename Stream::char_type;
		using traits_type = typename Stream::traits_type;
		using int_type = typename Stream::int_type;
		using pos_type = typename Stream::pos_type;
		using off_type = typename Stream::off_type;
		using string_type = std::basic_string<char_type, traits_type>;

		constexpr stream_adapter(stream_type&& stream) noexcept : _stream(std::move(stream)) {}
		constexpr stream_adapter(stream_adapter<Stream>&& stream) noexcept : _stream(std::move(stream._stream)) {}

		inline task<std::size_t> read(char_type* dst, std::size_t size) {
			return _stream.read(dst, size);
		}

		inline task<std::optional<char_type>> get() {
			return _stream.get();
		}

		inline task<std::optional<char_type>> peek()
			requires AsyncPeekableStream<Stream>
		{
			return _stream.peek();
		}

		constexpr auto take(std::size_t count) && noexcept {
			return stream_adapter<take_stream<stream_type>>(take_stream<stream_type>(std::move(_stream), count));
		}

		template <class UnaryPredicate>
		constexpr auto take_while(UnaryPredicate p) && noexcept {
			return stream_adapter<take_while_stream<stream_type, UnaryPredicate>>(
				take_while_stream<stream_type, UnaryPredicate>(std::move(_stream), p));
		}

		constexpr stream_type into_inner() && noexcept {
			return std::move(_stream);
		}

		task<string_type> collect() && {
			string_type result;
			char_type buffer[1024];
			std::size_t nread = 0;

			while ((nread = co_await _stream.read(buffer, 1024)) > 0) {
				result.append(buffer, buffer + nread);
			}
			co_return result;
		}
	};

	template <AsyncReadableStream Stream>
	constexpr stream_adapter<Stream> make_adapter(Stream&& stream) noexcept {
		return stream_adapter<Stream>(std::move(stream));
	}

	template <AsyncReadableStream Stream>
	constexpr stream_adapter<stream_reference<Stream>> wrap_adapter(Stream& stream) noexcept {
		return make_adapter(wrap_stream(stream));
	}
} // namespace cobra
#endif
