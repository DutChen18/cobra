#ifndef COBRA_HTTP_WRITER_HH
#define COBRA_HTTP_WRITER_HH

#include "cobra/asyncio/stream.hh"
#include "cobra/asyncio/stream_buffer.hh"
#include "cobra/compress/deflate.hh"
#include "cobra/http/message.hh"
#include "cobra/net/stream.hh"

#include <type_traits>

namespace cobra {
	template <AsyncBufferedInputStream Stream>
	using http_istream_variant = istream_variant<
		Stream,
		istream_limit<Stream>,
		inflate_istream<Stream>,
		istream_limit<inflate_istream<Stream>>
	>;

	template <AsyncBufferedOutputStream Stream>
	using http_ostream_variant = ostream_variant<
		Stream,
		ostream_limit<Stream>,
		deflate_ostream<Stream>,
		ostream_limit<deflate_ostream<Stream>>
	>;

	static_assert(std::is_swappable_v<buffered_ostream_reference>);
	static_assert(std::is_swappable_v<ostream_limit<buffered_ostream_reference>>);
	static_assert(std::is_swappable_v<deflate_ostream<buffered_ostream_reference>>);

	using http_istream = istream_ref<http_istream_variant<buffered_istream_reference>>;
	using http_ostream = ostream_ref<http_ostream_variant<buffered_ostream_reference>>;

	class http_server_logger {
		const basic_socket_stream* _socket = nullptr;

	public:
		void set_socket(const basic_socket_stream& socket);

		void log(const http_request* request, const http_response& response);
	};

	class http_ostream_wrapper {
		http_ostream_variant<buffered_ostream_reference> _stream;

	public:
		http_ostream_wrapper(buffered_ostream_reference stream);

		buffered_ostream_reference inner();
		http_ostream get();
		http_ostream get(std::size_t limit);
		http_ostream get_deflate();
		http_ostream get_deflate(std::size_t limit);
		task<void> end();
	};

	class http_request_writer {
		http_ostream_wrapper* _stream;

	public:
		http_request_writer(http_ostream_wrapper* stream);

		task<http_ostream> send(http_request request)&&;
	};

	class http_response_writer {
		const http_request* _request;
		http_ostream_wrapper* _stream;
		http_server_logger* _logger;

	public:
		http_response_writer(const http_request* request, http_ostream_wrapper* stream, http_server_logger* logger = nullptr);

		task<http_ostream> send(http_response response)&&;
	};

	task<void> write_http_request(ostream_reference stream, const http_request& request);
	task<void> write_http_response(ostream_reference stream, const http_response& response);
}

#endif
