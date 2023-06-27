#ifndef COBRA_ASYNCIO_STREAM_UTILS_HH
#define COBRA_ASYNCIO_STREAM_UTILS_HH

#include "cobra/asyncio/stream.hh"

namespace cobra {
	template<class Stream>
	class stream_adapter : public basic_istream<typename Stream::char_type, typename Stream::traits_type> {
	public:
		using char_type = typename Stream::char_type;
		using traits_type = typename Stream::traits_type;
		using int_type = typename Stream::int_type;
		using pos_type = typename Stream::pos_type;
		using off_type = typename Stream::off_type;

	};

	template<class Stream>
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

		task<std::size_t> read(char_type *data, std::size_t size) override {
			std::size_t nread = _stream.read(data, size < _n ? size : _n);
			_n -= nread;
			co_return nread;
		}
	};

	template<class Stream>
	[[nodiscard]] take_stream<Stream> take(Stream stream, std::size_t count) {
		return take_stream<Stream>(stream, count);
	}

	template<class PeekableStream, class UnaryPredicate>
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

		task<std::size_t> read(char_type *data, std::size_t size) override {
			std::size_t nwritten = 0;
			while (!_flag && nwritten < size) {
				auto ch = co_await _stream.peek();
				if (_p(*ch)) {
					data[nwritten] = *ch;
					++nwritten;
				} else {
					_flag = true;
					break;
				}
			}
			co_return nwritten;
		}
	};

	template<class PeekableStream, class UnaryPredicate>
	[[nodiscard]] take_while_stream<PeekableStream, UnaryPredicate> take_while(PeekableStream stream, UnaryPredicate p) {
		return take_while_stream(stream, p);
	}

	template<class Stream, class String = std::basic_string<typename Stream::char_type, typename Stream::traits_type>>
	task<String> collect(Stream&& stream) {
		String result;
		typename Stream::char_type buffer[1024];
		std::size_t nread;

		while ((nread = co_await stream.read(buffer, 1024)) > 0) {
			result.append(buffer, buffer + nread);
		}
		co_return result;
	}

	template<class Stream, class String = std::basic_string<typename Stream::char_type, typename Stream::traits_type>>
	task<String> collect(Stream& stream) {
		String result;
		typename Stream::char_type buffer[1024];
		std::size_t nread;

		while ((nread = co_await stream.read(buffer, 1024)) > 0) {
			result.append(buffer, buffer + nread);
		}
		co_return result;
	}
}
#endif
