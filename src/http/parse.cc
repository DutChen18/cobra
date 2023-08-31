#include "cobra/http/parse.hh"

#include "cobra/http/util.hh"
#include "cobra/print.hh"

#include <concepts>
#include <format>
#include <functional>

namespace cobra {
	static task<char> peek(buffered_istream_reference stream) {
		if (auto ch = co_await stream.peek()) {
			co_return *ch;
		} else {
			throw http_parse_error::unexpected_eof;
		}
	}

	template <std::predicate<char> UnaryPredicate>
	static task<bool> take(buffered_istream_reference stream, UnaryPredicate pred) {
		if (pred(co_await peek(stream))) {
			stream.consume(1);
			co_return true;
		} else {
			co_return false;
		}
	}

	static task<bool> take(buffered_istream_reference stream, char ch) {
		return take(stream, std::bind_front(std::equal_to(), ch));
	}

	static task<bool> take(buffered_istream_reference stream, std::string_view str) {
		for (char ch : str) {
			if (!co_await take(stream, ch)) {
				co_return false;
			}
		}

		co_return true;
	}

	static void assert(bool condition, http_parse_error error) {
		if (!condition) {
			throw error;
		}
	}

	static task<bool> parse_http_eol(buffered_istream_reference stream) {
		bool cr = co_await take(stream, '\r');
		assert(co_await take(stream, '\n') == cr, http_parse_error::bad_eol);
		co_return cr;
	}

	static task<bool> parse_cgi_eol(buffered_istream_reference stream) {
		bool cr = co_await take(stream, '\r');
		bool lf = co_await take(stream, '\n');
		co_return cr || lf;
	}

	template <std::predicate<char> UnaryPredicate>
	static task<std::string> parse_http_string(buffered_istream_reference stream, UnaryPredicate pred,
											   std::size_t max_length, http_parse_error error) {
		std::string result;

		while (pred(co_await peek(stream))) {
			result.push_back(*co_await stream.get());
			assert(result.size() <= max_length, error);
		}

		co_return result;
	}

	static task<int> parse_http_digit(buffered_istream_reference stream, http_parse_error error) {
		char ch = *co_await stream.get();
		assert(ch >= '0' && ch <= '9', error);
		co_return ch - '0';
	}

	static task<http_header_key> parse_http_header_key(buffered_istream_reference stream) {
		http_header_key key = co_await parse_http_string(stream, is_http_token, http_header_key_max_length,
														 http_parse_error::header_key_too_long);
		assert(co_await take(stream, ':'), http_parse_error::bad_header_key);
		assert(!key.empty(), http_parse_error::empty_header_key);
		co_return key;
	}

	static task<http_header_value> parse_http_header_value(buffered_istream_reference stream) {
		http_header_value value;
		bool space = false;

		while (true) {
			if (co_await take(stream, is_http_ws)) {
				space = !value.empty();
			} else if (!is_http_ctl(co_await peek(stream))) {
				if (std::exchange(space, false)) {
					value.push_back(' ');
				}

				value.push_back(*co_await stream.get());
				assert(value.size() <= http_header_value_max_length, http_parse_error::header_value_too_long);
			} else if (co_await parse_http_eol(stream)) {
				if (!is_http_ws(co_await peek(stream))) {
					co_return value;
				}
			} else {
				throw http_parse_error::bad_header_value;
			}
		}
	}

	static task<void> parse_http_header_map(buffered_istream_reference stream, http_message& message) {
		std::size_t length = 0;
		std::size_t size = 0;

		while (is_http_token(co_await peek(stream))) {
			http_header_key key = co_await parse_http_header_key(stream);
			http_header_value value = co_await parse_http_header_value(stream);
			length += 1;
			size += value.size();

			// ODOT: Set-Cookie size
			if (!message.has_header(key)) {
				size += key.size();
			}

			assert(length <= http_header_map_max_length, http_parse_error::header_map_too_long);
			assert(size <= http_header_map_max_size, http_parse_error::header_map_too_large);
			message.add_header(std::move(key), std::move(value));
		}

		assert(co_await parse_http_eol(stream), http_parse_error::bad_header);
	}

	static task<http_header_key> parse_cgi_header_key(buffered_istream_reference stream) {
		http_header_key key = co_await parse_http_string(stream, is_http_token, cgi_header_key_max_length,
														 http_parse_error::header_key_too_long);
		assert(co_await take(stream, ':'), http_parse_error::bad_header_key);
		assert(!key.empty(), http_parse_error::empty_header_key);
		co_return key;
	}

	static task<http_header_value> parse_cgi_header_value(buffered_istream_reference stream) {
		http_header_value value = co_await parse_http_string(stream, is_cgi_value, cgi_header_value_max_length,
															 http_parse_error::header_value_too_long);
		assert(co_await parse_cgi_eol(stream), http_parse_error::bad_header_value);
		co_return value;
	}

	static task<http_header_map> parse_cgi_header_map(buffered_istream_reference stream) {
		http_header_map map;
		std::size_t length = 0;
		std::size_t size = 0;

		while (is_http_token(co_await peek(stream))) {
			http_header_key key = co_await parse_cgi_header_key(stream);
			http_header_value value = co_await parse_cgi_header_value(stream);
			length += 1;
			size += value.size();

			// ODOT: Set-Cookie size
			if (!map.contains(key)) {
				size += key.size();
			}

			assert(length <= cgi_header_map_max_length, http_parse_error::header_map_too_long);
			assert(size <= cgi_header_map_max_size, http_parse_error::header_map_too_large);

			std::size_t begin = value.find_first_not_of(" \t");

			if (begin == std::string::npos) {
				value.clear();
			} else {
				std::size_t end = value.find_last_not_of(" \t") + 1;
				value = value.substr(begin, end - begin);
			}

			// php sometimes sends the same header field multiple times, this is non-standard
			map.insert(std::move(key), std::move(value));
		}

		assert(co_await parse_cgi_eol(stream), http_parse_error::bad_header);
		co_return map;
	}

	static task<http_version> parse_http_version(buffered_istream_reference stream) {
		assert(co_await take(stream, "HTTP/"), http_parse_error::bad_version);
		http_version_type major = co_await parse_http_digit(stream, http_parse_error::bad_version);
		assert(co_await take(stream, '.'), http_parse_error::bad_version);
		http_version_type minor = co_await parse_http_digit(stream, http_parse_error::bad_version);
		co_return http_version(major, minor);
	}

	static uri_abs_path parse_uri_abs_path(std::string_view string) {
		std::vector<std::string> segments;
		std::string segment;

		if (!string.starts_with("/")) {
			throw uri_parse_error::bad_uri;
		}

		for (std::size_t i = 0; i < string.size(); i++) {
			if (string[i] == '/') {
				if (!segment.empty()) {
					segments.emplace_back(std::move(segment));
					segment = std::string();
				} else if (i != 0) {
					throw uri_parse_error::bad_uri;
				}
			} else if (string[i] == '%') {
				if (string.size() - i < 3) {
					throw uri_parse_error::bad_escape;
				}

				if (auto hi = unhexify(string[++i])) {
					if (auto lo = unhexify(string[++i])) {
						segment.push_back(*hi << 4 | *lo);
					} else {
						throw uri_parse_error::bad_escape;
					}
				} else {
					throw uri_parse_error::bad_escape;
				}
			} else if (is_uri_segment(string[i])) {
				segment.push_back(string[i]);
			} else {
				throw uri_parse_error::bad_segment;
			}
		}

		if (!segment.empty() || !segments.empty()) {
			segments.emplace_back(std::move(segment));
		}

		return uri_abs_path(std::move(segments));
	}

	static uri_query parse_uri_query(std::string_view string) {
		for (std::size_t i = 0; i < string.size(); i++) {
			if (!is_uri_query(string[i])) {
				throw uri_parse_error::bad_query;
			}
		}

		return std::string(string);
	}

	uri_origin parse_uri_origin(std::string_view string) {
		std::string_view::size_type query_begin = string.find('?');

		if (query_begin != std::string_view::npos) {
			uri_abs_path path = parse_uri_abs_path(string.substr(0, query_begin));
			uri_query query = parse_uri_query(string.substr(query_begin + 1));
			return uri_origin(path, query);
		} else {
			return uri_origin(parse_uri_abs_path(string), std::nullopt);
		}
	}

	uri_absolute parse_uri_absolute(std::string_view string) {
		return uri_absolute(std::string(string)); // ODOT: not checked
	}

	uri_authority parse_uri_authority(std::string_view string) {
		return uri_authority(std::string(string)); // ODOT: not checked
	}

	uri_asterisk parse_uri_asterisk(std::string_view string) {
		if (string != "*") {
			throw uri_parse_error::bad_asterisk;
		}

		return uri_asterisk();
	}

	uri parse_uri(std::string_view string, const http_request_method& method) {
		if (method == "CONNECT") {
			return parse_uri_authority(string);
		} else if (string.starts_with("*") && method == "OPTIONS") {
			return parse_uri_asterisk(string);
		} else if (string.starts_with("/")) {
			return parse_uri_origin(string);
		} else {
			return parse_uri_absolute(string);
		}
	}

	task<http_request> parse_http_request(buffered_istream_reference stream) {
		http_request_method method = co_await parse_http_string(stream, is_http_token, http_request_method_max_length,
																http_parse_error::request_method_too_long);
		assert(co_await take(stream, ' '), http_parse_error::bad_request_method);
		assert(!method.empty(), http_parse_error::empty_request_method);

		std::string uri = co_await parse_http_string(stream, is_http_uri, http_request_uri_max_length,
													 http_parse_error::request_uri_too_long);
		assert(co_await take(stream, ' '), http_parse_error::bad_request_uri);

		http_version version = co_await parse_http_version(stream);
		assert(co_await parse_http_eol(stream), http_parse_error::bad_version);

		http_request request(version, method, parse_uri(uri, method));
		co_await parse_http_header_map(stream, request);
		co_return request;
	}

	task<http_response> parse_http_response(buffered_istream_reference stream) {
		http_version version = co_await parse_http_version(stream);
		assert(co_await take(stream, ' '), http_parse_error::bad_version);

		http_response_code code = 0;
		code += co_await parse_http_digit(stream, http_parse_error::bad_response_code) * 100;
		code += co_await parse_http_digit(stream, http_parse_error::bad_response_code) * 10;
		code += co_await parse_http_digit(stream, http_parse_error::bad_response_code) * 1;
		assert(co_await take(stream, ' '), http_parse_error::bad_response_code);

		http_response_reason reason = co_await parse_http_string(
			stream, is_http_reason, http_response_reason_max_length, http_parse_error::response_reason_too_long);
		assert(co_await parse_http_eol(stream), http_parse_error::bad_response_reason);

		http_response response(version, code, reason);
		co_await parse_http_header_map(stream, response);
		co_return response;
	}

	task<http_header_map> parse_cgi(buffered_istream_reference stream) {
		return parse_cgi_header_map(stream);
	}
} // namespace cobra
