#ifndef COBRA_HTTP_CONTEXT_HH
#define COBRA_HTTP_CONTEXT_HH

#include "cobra/asyncio/stream.hh"
#include "cobra/net/stream.hh"

namespace cobra {
	class http_context : public istream_impl<http_context>, public ostream_impl<http_context> {
		socket_stream* _stream;
		std::size_t _read_size;
		std::size_t _write_size;

	public:
		task<std::size_t> read(char_type* data, std::size_t size);
		task<std::size_t> write(const char_type* data, std::size_t size);
		task<void> flush();
	};
}

#endif
