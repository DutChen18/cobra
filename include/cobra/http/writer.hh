#ifndef COBRA_HTTP_WRITER_HH
#define COBRA_HTTP_WRITER_HH

#include "cobra/asyncio/null_stream.hh"
#include "cobra/asyncio/stream.hh"
#include "cobra/asyncio/stream_buffer.hh"
#include "cobra/compress/deflate.hh"
#include "cobra/http/message.hh"
#include "cobra/http/parse.hh"
#include "cobra/http/util.hh"
#include "cobra/net/stream.hh"

#include <type_traits>

namespace cobra {
	template <AsyncBufferedInputStream Stream>
	class chunked_istream : public buffered_istream_impl<chunked_istream<Stream>> {
		Stream _stream;
		std::size_t _remaining = 0;
		bool _done = false;
		int _crlf = 0;

	public:
		using typename buffered_istream_impl<chunked_istream<Stream>>::char_type;

		chunked_istream(Stream&& stream) : _stream(std::move(stream)) {}

		task<std::pair<const char_type*, std::size_t>> fill_buf() {
			while (true) {
				while (_crlf > 0) {
					if (co_await _stream.get() != '\r') {
						throw http_parse_error::bad_content;
					}

					if (co_await _stream.get() != '\n') {
						throw http_parse_error::bad_content;
					}

					_crlf -= 1;
				}

				if (_done) {
					co_return {nullptr, 0};
				}

				if (_remaining > 0) {
					auto [data, size] = co_await _stream.fill_buf();
					co_return {data, std::min(size, _remaining)};
				}

				if (auto digit = unhexify(co_await _stream.peek())) {
					_stream.consume(1);
					_remaining = *digit;
				} else {
					throw http_parse_error::bad_content;
				}

				while (auto digit = unhexify(co_await _stream.peek())) {
					_stream.consume(1);
					_remaining = _remaining * 16 + *digit;
				}

				if (_remaining == 0) {
					_done = true;
					_crlf += 1;
				}

				_crlf += 1;
			}
		}

		void consume(std::size_t size) {
			_stream.consume(size);
			_remaining -= size;

			if (size > 0 && _remaining == 0) {
				_crlf += 1;
			}
		}

		task<Stream> end() && {
			while (!_done) {
				auto [data, size] = co_await fill_buf();

				if (size == 0) {
					throw stream_error::incomplete_read;
				}

				consume(size);
			}

			co_return std::move(_stream);
		}
	};

	template <AsyncOutputStream Stream>
	class chunked_ostream : public ostream_impl<chunked_ostream<Stream>> {
		Stream _stream;

	public:
		using typename ostream_impl<chunked_ostream<Stream>>::char_type;

		chunked_ostream(Stream&& stream) : _stream(std::move(stream)) {}

		task<std::size_t> write(const char_type* data, std::size_t size) {
			if (size != 0) {
				std::size_t tmp_size = size;
				std::size_t index = 0;
				char buffer[66];

				while (tmp_size > 0) {
					buffer[64 - ++index] = "0123456789abcdef"[tmp_size % 16];
					tmp_size /= 16;
				}

				buffer[64] = '\r';
				buffer[65] = '\n';

				co_await _stream.write_all(buffer + 64 - index, index + 2);
				co_await _stream.write_all(data, size);
				co_await _stream.write_all("\r\n", 2);
			}

			co_return size;
		}

		task<void> flush() {
			return _stream.flush();
		}

		task<Stream> end() && {
			co_await _stream.write_all("0\r\n\r\n", 5);
			co_return std::move(_stream);
		}
	};

	template <AsyncBufferedInputStream Stream>
	using http_istream_variant =
		buffered_istream_variant<Stream, chunked_istream<Stream>, istream_limit<Stream>, inflate_istream<Stream>,
								 inflate_istream<chunked_istream<Stream>>, istream_limit<inflate_istream<Stream>>>;

	template <AsyncBufferedOutputStream Stream>
	using http_ostream_variant =
		buffered_ostream_variant<Stream, ostream_buffer<chunked_ostream<Stream>>, ostream_limit<Stream>,
								 deflate_ostream<Stream>, deflate_ostream<ostream_buffer<chunked_ostream<Stream>>>,
								 ostream_limit<deflate_ostream<Stream>>>;

	using http_istream = buffered_istream_ref<http_istream_variant<buffered_istream_reference>>;
	using http_ostream =
		buffered_ostream_variant<buffered_ostream_ref<http_ostream_variant<buffered_ostream_reference>>, null_ostream>;

	bool has_header_value(const http_message& message, const std::string& key, std::string_view target);

	template <AsyncBufferedInputStream Stream>
	http_istream_variant<Stream> get_istream(Stream stream, const http_message& message) {
		if (message.has_header("Content-Length")) {
			std::size_t size = std::stoull(message.header("Content-Length"));
			return istream_limit(std::move(stream), size);
		} else if (has_header_value(message, "Transfer-Encoding", "chunked")) {
			return chunked_istream(std::move(stream));
		} else {
			return std::move(stream);
		}
	}

	class http_server_logger {
		const basic_socket_stream* _socket = nullptr;

	public:
		void set_socket(const basic_socket_stream& socket);

		void log(const http_request* request, const http_response& response);
	};

	class http_ostream_wrapper {
		http_ostream_variant<buffered_ostream_reference> _stream;
		bool _keep_alive = true;
		bool _sent = false;

	public:
		http_ostream_wrapper(buffered_ostream_reference stream);

		buffered_ostream_reference inner();
		http_ostream get();
		http_ostream get_chunked();
		http_ostream get(std::size_t limit);
		http_ostream get_deflate();
		http_ostream get_deflate_chunked();
		http_ostream get_deflate(std::size_t limit);
		task<void> end();

		void set_close();
		void set_sent();
		bool keep_alive() const;

		inline bool sent() const {
			return _sent;
		}
	};

	class http_request_writer {
		http_ostream_wrapper* _stream;

	public:
		http_request_writer(http_ostream_wrapper* stream);

		task<http_ostream> send(http_request request) &&;
	};

	class http_response_writer {
		const http_request* _request;
		http_ostream_wrapper* _stream;
		http_server_logger* _logger;
		std::vector<std::pair<std::string, std::string>> _headers;

	public:
		http_response_writer(const http_request* request, http_ostream_wrapper* stream,
							 http_server_logger* logger = nullptr);

		void set_header(std::string key, std::string value);

		bool can_compress() const;
		task<http_ostream> send(http_response response) &&;
	};

	task<void> write_http_request(ostream_reference stream, const http_request& request);
	task<void> write_http_response(ostream_reference stream, const http_response& response);
} // namespace cobra

#endif
