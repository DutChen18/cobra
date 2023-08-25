#include "cobra/http/writer.hh"
#include "cobra/print.hh"
#include "cobra/compress/deflate.hh"

namespace cobra {
	bool has_header_value(const http_message& message, const std::string& key, std::string_view target) {
		if (message.has_header(key)) {
			std::string_view value = message.header(key);
			std::string_view::size_type pos = 0;

			while (pos != std::string_view::npos) {
				std::string_view::size_type end = value.find(",", pos);
				std::string_view part = value.substr(pos, end == std::string_view::npos ? std::string_view::npos : end - pos);
				std::size_t begin = part.find_first_not_of(" \t");

				if (begin == std::string::npos) {
					part = std::string_view();
				} else {
					std::size_t end = part.find_last_not_of(" \t") + 1;
					part = part.substr(begin, end - begin);
				}

				if (std::equal(part.begin(), part.end(), target.begin(), target.end(), [](char a, char b) { return tolower(a) == tolower(b); })) {
					return true;
				}

				if (end == std::string_view::npos) {
					pos = end;
				} else {
					pos = end + 1;
				}
			}
		}

		return false;
	}

	static http_ostream to_stream(http_ostream_wrapper* stream, const http_message& message) {
		if (!has_header_value(message, "Connection", "keep-alive")) {
			stream->set_close();
		}

		if (has_header_value(message, "Content-Encoding", "deflate")) {
			if (message.has_header("Content-Length")) {
				std::size_t size = std::stoull(message.header("Content-Length"));
				return stream->get_deflate(size);
			} else if (has_header_value(message, "Transfer-Encoding", "chunked")) {
				return stream->get_deflate_chunked();
			} else {
				return stream->get_deflate();
			}
		} else {
			if (message.has_header("Content-Length")) {
				std::size_t size = std::stoull(message.header("Content-Length"));
				return stream->get(size);
			} else if (has_header_value(message, "Transfer-Encoding", "chunked")) {
				return stream->get_chunked();
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

	template <AsyncBufferedOutputStream Stream>
	static task<buffered_ostream_reference> end_stream(ostream_buffer<Stream>& stream) {
		co_await stream.flush();
		co_return co_await end_stream(stream.inner());
	}

	template <AsyncBufferedOutputStream Stream>
	static task<buffered_ostream_reference> end_stream(chunked_ostream<Stream>& stream) {
		Stream tmp = co_await std::move(stream).end();
		co_return co_await end_stream(tmp);
	}

	void http_server_logger::set_socket(const basic_socket_stream& socket) {
		_socket = &socket;
	}
	
	void http_server_logger::log(const http_request* request, const http_response& response) {
		term::control ctrl;
		std::stringstream ss;

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

		print(ss, "{}[{}]", ctrl, response.code());

		if (_socket) {
			print(ss, " {}", _socket->peername().string());
		}

		if (request) {
			if (_socket) {
				print(ss, " ->");
			}

			print(ss, " {} {}", request->method(), request->uri().string());
		}

		print(ss, "{}", term::reset());
		println("{}", ss.str());
	}

	http_ostream_wrapper::http_ostream_wrapper(buffered_ostream_reference stream) : _stream(std::move(stream)) {
	}

	buffered_ostream_reference http_ostream_wrapper::inner() {
		return std::get<buffered_ostream_reference>(_stream.variant());
	}

	http_ostream http_ostream_wrapper::get() {
		return _stream;
	}

	http_ostream http_ostream_wrapper::get_chunked() {
		_stream = ostream_buffer(chunked_ostream(inner()), COBRA_BUFFER_SIZE);
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

	http_ostream http_ostream_wrapper::get_deflate_chunked() {
		_stream = deflate_ostream(ostream_buffer(chunked_ostream(inner()), COBRA_BUFFER_SIZE), deflate_mode::zlib);
		return _stream;
	}

	http_ostream http_ostream_wrapper::get_deflate(std::size_t limit) {
		_stream = ostream_limit(deflate_ostream(inner(), deflate_mode::zlib), limit);
		return _stream;
	}

	task<void> http_ostream_wrapper::end() {
		assert(_sent);
		_sent = false;
		_stream = co_await std::visit([](auto& stream) { return end_stream(stream); }, _stream.variant());
	}

	void http_ostream_wrapper::set_close() {
		_keep_alive = false;
	}

	void http_ostream_wrapper::set_sent() {
		assert(!_sent);
		_sent = true;
	}

	bool http_ostream_wrapper::keep_alive() const {
		return _keep_alive;
	}

	http_request_writer::http_request_writer(http_ostream_wrapper* stream) : _stream(stream) {
	}

	task<http_ostream> http_request_writer::send(http_request request)&& {
		_stream->set_sent();
		co_await write_http_request(_stream->inner(), request);
		co_return to_stream(_stream, request);
	}

	http_response_writer::http_response_writer(const http_request* request, http_ostream_wrapper* stream, http_server_logger* logger) : _request(request), _stream(stream), _logger(logger) {
	}

	void http_response_writer::set_header(std::string key, std::string value) {
		_headers.push_back({ std::move(key), std::move(value) });
	}

	bool http_response_writer::can_compress() const {
		return _request && has_header_value(*_request, "Accept-Encoding", "deflate");
	}

	task<http_ostream> http_response_writer::send(http_response response)&& {
		for (const auto& [key, value] : _headers) {
			response.set_header(key, value);
		}

		if (response.code() != HTTP_SWITCHING_PROTOCOLS) {
			if (!response.has_header("Content-Encoding") && !response.has_header("Content-Length") && can_compress()) {
				response.set_header("Content-Encoding", "deflate");
			}

			if (!response.has_header("Transfer-Encoding") && !response.has_header("Content-Length")) {
				response.set_header("Transfer-Encoding", "chunked");
			}

			if (!response.has_header("Connection")) {
				if (_request && has_header_value(*_request, "Connection", "keep-alive") && (response.has_header("Content-Length") || has_header_value(response, "Transfer-Encoding", "chunked"))) {
					response.set_header("Connection", "keep-alive");
				} else {
					response.set_header("Connection", "close");
				}
			}
		}

		if (_logger) {
			_logger->log(_request, response);
		}

		_stream->set_sent();
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
