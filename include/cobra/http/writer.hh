#ifndef COBRA_HTTP_WRITER_HH
#define COBRA_HTTP_WRITER_HH

#include "cobra/asyncio/stream.hh"
#include "cobra/asyncio/stream_buffer.hh"
#include "cobra/http/message.hh"
#include "cobra/net/stream.hh"

namespace cobra {
	using http_istream = istream_variant<buffered_istream_reference, istream_limit<buffered_istream_reference>>;
	using http_ostream = ostream_variant<buffered_ostream_reference, ostream_limit<buffered_ostream_reference>>;

	class http_server_logger {
		const socket_stream* _socket = nullptr;
		const http_request* _request = nullptr;

	public:
		void set_socket(const socket_stream& socket);
		void set_request(const http_request& request);

		void log(const http_response& response);
	};

	class http_request_writer {
		buffered_ostream_reference _stream;

	public:
		http_request_writer(buffered_ostream_reference stream);

		task<http_ostream> send(http_request request)&&;
	};

	class http_response_writer {
		buffered_ostream_reference _stream;
		http_server_logger* _logger;

	public:
		http_response_writer(buffered_ostream_reference stream, http_server_logger* logger = nullptr);

		task<http_ostream> send(http_response response)&&;
	};

	task<void> write_http_request(ostream_reference stream, const http_request& request);
	task<void> write_http_response(ostream_reference stream, const http_response& response);
}

#endif
