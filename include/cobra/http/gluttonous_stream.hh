#ifndef COBRA_HTTP_GLUTTONOUS_STREAM_HH
#define COBRA_HTTP_GLUTTONOUS_STREAM_HH

#include "cobra/asyncio/stream.hh"
#include "cobra/http/message.hh"

namespace cobra {
	template <AsyncBufferedInputStream Stream>
	class gluttonous_stream : public buffered_istream_impl<gluttonous_stream<Stream>> {
		Stream _stream;
		std::optional<std::size_t> _limit;

	public:
		using typename buffered_istream_impl<gluttonous_stream<Stream>>::char_type;

		gluttonous_stream(Stream&& stream) : _stream(std::move(stream)) {}

		gluttonous_stream(Stream&& stream, std::size_t limit) : _stream(std::move(stream)), _limit(limit) {}

		gluttonous_stream(Stream&& stream, std::optional<std::size_t> limit)
			: _stream(std::move(stream)), _limit(limit) {}

		task<std::pair<const char_type*, std::size_t>> fill_buf() {
			auto [data, size] = co_await _stream.fill_buf();

			if (_limit && size > *_limit) {
				throw HTTP_CONTENT_TOO_LARGE;
			}

			co_return {data, size};
		}

		void consume(std::size_t size) {
			if (_limit) {
				*_limit -= size;
			}

			_stream.consume(size);
		}
	};
} // namespace cobra

#endif
