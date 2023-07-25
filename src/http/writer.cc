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

	void http_server_logger::set_socket(const socket_stream& socket) {
		_socket = &socket;
	}

	void http_server_logger::set_request(const http_request& request) {
		_request = &request;
	}

	void http_server_logger::log(const http_response& response) {
		term::control ctrl;

		if (response.code() / 100 == 1) {
			ctrl |= term::fg_cyan();
		} else if (response.code() / 100 == 2) {
			ctrl |= term::fg_green();
		} else if (response.code() / 100 == 3) {
			ctrl |= term::fg_yellow();
		} else if (response.code() / 100 == 4) {
			ctrl |= term::fg_red();
		} else if (response.code() / 100 == 5) {
			ctrl |= term::fg_magenta();
		}

		print("{}[{}]", ctrl, response.code());

		if (_socket) {
			print(" {}", _socket->peername().string());
		}

		if (_request) {
			if (_socket) {
				print(" ->");
			}

			print(" {} {}", _request->method(), _request->uri().string());
		}

		println("{}", term::reset());
	}

	http_request_writer::http_request_writer(buffered_ostream_reference stream) : _stream(stream) {
	}

	task<http_ostream> http_request_writer::send(http_request request)&& {
		co_await write_http_request(_stream, request);
		co_return to_stream(_stream, request);
	}

	http_response_writer::http_response_writer(buffered_ostream_reference stream, http_server_logger* logger) : _stream(stream), _logger(logger) {
	}

	// TODO: implement keep-alive
	task<http_ostream> http_response_writer::send(http_response response)&& {
		response.set_header("Connection", "close");

		if (_logger) {
			_logger->log(response);
		}

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
