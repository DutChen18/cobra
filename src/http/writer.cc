#include "cobra/http/writer.hh"
#include "cobra/print.hh"

namespace cobra {
	static http_ostream to_stream(buffered_ostream_reference stream, const http_message& message) {
		if (message.has_header("Content-Length")) {
			std::size_t size = std::stoull(message.header("Content-Length"));
			return ostream_limit(std::move(stream), size);
		} else {
			return stream;
		}
	}

	http_request_writer::http_request_writer(buffered_ostream_reference stream) : _stream(stream) {
	}

	task<http_ostream> http_request_writer::send(const http_request& request)&& {
		co_await write_http_request(_stream, request);
		co_return to_stream(_stream, request);
	}

	http_response_writer::http_response_writer(buffered_ostream_reference stream) : _stream(stream) {
	}

	task<http_ostream> http_response_writer::send(const http_response& response)&& {
		co_await write_http_response(_stream, response);
		co_return to_stream(_stream, response);
	}

	static task<void> write_http_header_map(ostream_reference stream, const http_message& message) {
		for (const auto& [key, value] : message.header_map()) {
			co_await print(stream, "{}: {}\r\n", key, value);
		}

		co_await print(stream, "\r\n");
	}
	
	task<void> write_http_request(ostream_reference stream, const http_request& request) {
		co_await print(stream, "{} {} HTTP/{}.{}\r\n", request.method(), request.uri().string(), request.version().major(), request.version().minor());
		co_await write_http_header_map(stream, request);
		co_await stream.flush();
	}

	task<void> write_http_response(ostream_reference stream, const http_response& response) {
		co_await print(stream, "HTTP/{}.{} {} {}\r\n", response.version().major(), response.version().minor(), response.code(), response.reason());
		co_await write_http_header_map(stream, response);
		co_await stream.flush();
	}
}
