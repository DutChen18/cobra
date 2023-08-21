#include "cobra/http/writer.hh"
#include "cobra/print.hh"
#include "cobra/compress/deflate.hh"

namespace cobra {
	static http_ostream to_stream(http_ostream_wrapper* stream, const http_message& message) {
		// TODO: trim
		if (message.has_header("Content-Encoding") && message.header("Content-Encoding") == "deflate") {
			if (message.has_header("Content-Length")) {
				std::size_t size = std::stoull(message.header("Content-Length"));
				return stream->get_deflate(size);
			} else {
				return stream->get_deflate();
			}
		} else {
			if (message.has_header("Content-Length")) {
				std::size_t size = std::stoull(message.header("Content-Length"));
				return stream->get(size);
			} else {
				return stream->get();
			}
		}
	}

	static task<buffered_ostream_reference> end_stream(buffered_ostream_reference& stream) {
		co_await stream.flush();
		co_return std::move(stream);
	}

	template <AsyncBufferedOutputStream Stream>
	static task<buffered_ostream_reference> end_stream(deflate_ostream<Stream>& stream) {
		Stream tmp = co_await std::move(stream).end();
		co_return co_await end_stream(tmp);
	}

	template <AsyncBufferedOutputStream Stream>
	static task<buffered_ostream_reference> end_stream(ostream_limit<Stream>& stream) {
		co_return co_await end_stream(stream.inner());
	}

	void http_server_logger::set_socket(const basic_socket_stream& socket) {
		_socket = &socket;
	}
	
	void http_server_logger::log(const http_request* request, const http_response& response) {
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

		if (request) {
			if (_socket) {
				print(" ->");
			}

			print(" {} {}", request->method(), request->uri().string());
		}

		println("{}", term::reset());
	}

	http_ostream_wrapper::http_ostream_wrapper(buffered_ostream_reference stream) : _stream(std::move(stream)) {
	}

	buffered_ostream_reference http_ostream_wrapper::inner() {
		return std::get<buffered_ostream_reference>(_stream.variant());
	}

	http_ostream http_ostream_wrapper::get() {
		return _stream;
	}

	http_ostream http_ostream_wrapper::get(std::size_t limit) {
		_stream = ostream_limit(inner(), limit);
		return _stream;
	}

	http_ostream http_ostream_wrapper::get_deflate() {
		_stream = deflate_ostream(inner(), deflate_mode::zlib);
		return _stream;
	}

	http_ostream http_ostream_wrapper::get_deflate(std::size_t limit) {
		_stream = ostream_limit(deflate_ostream(inner(), deflate_mode::zlib), limit);
		return _stream;
	}

	task<void> http_ostream_wrapper::end() {
		_stream = co_await std::visit([](auto& stream) { return end_stream(stream); }, _stream.variant());
	}

	http_request_writer::http_request_writer(http_ostream_wrapper* stream) : _stream(stream) {
	}

	task<http_ostream> http_request_writer::send(http_request request)&& {
		co_await write_http_request(_stream->inner(), request);
		co_return to_stream(_stream, request);
	}

	http_response_writer::http_response_writer(const http_request* request, http_ostream_wrapper* stream, http_server_logger* logger) : _request(request), _stream(stream), _logger(logger) {
	}

	// TODO: implement keep-alive
	task<http_ostream> http_response_writer::send(http_response response)&& {
		if (!response.has_header("Connection")) {
			response.set_header("Connection", "close");
		}

		if (_request) {
			if (_request->has_header("Accept-Encoding") && !response.has_header("Encoding")) {
				std::string_view accept_encoding = _request->header("Accept-Encoding");
				std::string_view::size_type pos = 0;

				while (pos != std::string_view::npos) {
					std::string_view::size_type end = accept_encoding.find(",", pos);
					std::string_view encoding = accept_encoding.substr(pos, end == std::string_view::npos ? std::string_view::npos : end - pos);

					std::size_t begin = encoding.find_first_not_of(" \t");

					if (begin == std::string::npos) {
						encoding = std::string_view();
					} else {
						std::size_t end = encoding.find_last_not_of(" \t") + 1;
						encoding = encoding.substr(begin, end - begin);
					}

					if (encoding == "deflate") {
						response.set_header("Content-Encoding", "deflate");
						break;
					}

					pos = end + 1;
				}
			}
		}

		if (_logger) {
			_logger->log(_request, response);
		}

		co_await write_http_response(_stream->inner(), response);
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
