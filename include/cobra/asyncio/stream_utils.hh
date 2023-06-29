#ifndef COBRA_ASYNCIO_STREAM_UTILS_HH
#define COBRA_ASYNCIO_STREAM_UTILS_HH

#include "cobra/asyncio/stream.hh"
#include <optional>

namespace cobra {

	template<AsyncReadableStream Stream>
	class take_stream : public basic_istream<typename Stream::char_type, typename Stream::traits_type> {
		Stream _stream;
		std::size_t _n;

	public:
		using char_type = typename Stream::char_type;
		using traits_type = typename Stream::traits_type;
		using int_type = typename Stream::int_type;
		using pos_type = typename Stream::pos_type;
		using off_type = typename Stream::off_type;

		take_stream(Stream stream, std::size_t count) : _stream(stream), _n(count) {}

		[[nodiscard]] task<std::size_t> read(char_type *data, std::size_t size) override {
			std::size_t nread = co_await _stream.read(data, size < _n ? size : _n);
			_n -= nread;
			co_return nread;
		}

		[[nodiscard]] task<std::optional<char_type>> peek() requires AsyncPeekableStream<Stream> {
			if (_n == 0) {
				return std::nullopt;
			} else {
				return _stream.peek();
			}
		}
	};

	template<AsyncPeekableStream PeekableStream, class UnaryPredicate>
	class take_while_stream : public basic_istream<typename PeekableStream::char_type, typename PeekableStream::traits_type> {
		PeekableStream _stream;
		UnaryPredicate _p;
		bool _flag;

	public:
		using char_type = typename PeekableStream::char_type;
		using traits_type = typename PeekableStream::traits_type;
		using int_type = typename PeekableStream::int_type;
		using pos_type = typename PeekableStream::pos_type;
		using off_type = typename PeekableStream::off_type;

		take_while_stream(PeekableStream stream, UnaryPredicate p) : _stream(stream), _p(p), _flag(false) {}

		[[nodiscard]] task<std::size_t> read(char_type *data, std::size_t size) override {
			std::size_t nwritten = 0;
			std::optional<char_type> ch;
			
			while ((ch = co_await peek())) {
				data[nwritten] = *ch;
				++nwritten;
			}
			co_return nwritten;
		}

		[[nodiscard]] task<std::optional<char_type>> peek() {
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

	template<AsyncReadableStream Stream>
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

		stream_adapter(stream_type&& stream) : _stream(stream) {}
		stream_adapter(stream_adapter<Stream>&& stream) : _stream(std::move(stream._stream)) {}

		[[nodiscard]] task<std::size_t> read(char_type* dst, std::size_t size) {
			return _stream.read(dst, size);
		}

		[[nodiscard]] task<std::optional<char_type>> peek() requires AsyncPeekableStream<Stream> {
			return _stream.peek();
		}

		[[nodiscard]] auto take(std::size_t count) && {
			return stream_adapter<take_stream<stream_type>>(take_stream<stream_type>(std::move(_stream), count));
		}

		template <class UnaryPredicate>
		[[nodiscard]] auto take_while(UnaryPredicate p) && {
			return stream_adapter<take_while_stream<stream_type, UnaryPredicate>>(take_while_stream<stream_type, UnaryPredicate>(std::move(_stream), p));
		}

		[[nodiscard]] task<string_type> collect() && {
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
	stream_adapter<Stream> make_adapter(Stream stream) {
		return stream_adapter<Stream>(std::move(stream));
	}

	template <AsyncReadableStream Stream>
	stream_adapter<basic_istream_reference<Stream>> wrap_adapter(Stream& stream) {
		return make_adapter(wrap(stream));
	}
}
#endif
