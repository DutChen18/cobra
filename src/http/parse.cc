#include "cobra/http.hh"
#include "cobra/future.hh"
#include "cobra/asio_utils.hh"
#include <exception>
#include <memory>
#include <cctype>

namespace cobra {

	http_version::http_version(version_type major, version_type minor) : _major(major), _minor(minor) {}

	http_method parse_method(const std::string& str) {
		if (str == "OPTIONS") {
			return http_method::options;
		} else if (str == "GET") {
			return http_method::get;
		} else if (str == "HEAD") {
			return http_method::head;
		} else if (str == "POST") {
			return http_method::post;
		} else if (str == "PUT") {
			return http_method::put;
		} else if (str == "DEL") {
			return http_method::del;
		} else if (str == "TRACE") {
			return http_method::trace;
		} else if (str == "CONNECT") {
			return http_method::connect;
		}
		return http_method::unknown;
	}

	/*
	static int htoi(int ch) {
		if (std::isdigit(ch))
			return ch - '0';
		return (std::toupper(ch) - 'A') + 10;
	}

	static bool is_mark(int ch) {
		return ch == '-' || ch == '_' || ch == '.' || ch == '!' || ch == '~' || ch == '*' || ch == '\'' ||
                      ch == '(' || ch == ')';
	}

	static bool is_hex(int ch) {
		return std::isdigit(ch) || (ch >= 'a' && ch <= 'f') || (ch >= 'A' || ch <= 'F');
	}

	static bool is_unreserved(int ch) {
		return std::isalnum(ch) || is_mark(ch);
	}

	static bool is_segment_char(int ch) {
		return is_unreserved(ch) || ch == '%' || ch == ';' || ch == ':'
			|| ch == '@' || ch == '&' || ch == '=' || ch == '+' || ch == '$'
			|| ch == ',';
	}

	static future<int> unescape(istream& stream) {
		return get_ch(stream).and_then<int>([&stream](int first_ch) {
			if (first_ch != '%')
				return resolve(std::move(first_ch));
			return get_ch(stream).and_then<int>([&stream](int second_ch) {
				if (!is_hex(second_ch))
					throw http_error(http_status_code::bad_request);
				return get_ch(stream).and_then<int>([second_ch](int third_ch) {
					if (!is_hex(third_ch))
						throw http_error(http_status_code::bad_request);
					return resolve(htoi(second_ch) * 16 + htoi(third_ch));
				});
			});
		});
	}

	future<std::string> parse_segments(buffered_istream& stream) {
		return async_while<std::string>(capture([&stream](std::string& segment) {
			return stream.peek().and_then<optional<std::string>>([&stream, &segment](optional<int> ch) {
				if (ch && ch == '/') {
					return stream.ignore(1).and_then<optional<std::string>>([&segment](std::size_t) {
						return resolve(some<std::string>(std::move(segment)));
					});
				} else if (ch && is_segment_char(*ch)) {
					return unescape(stream).and_then<optional<std::string>>([&segment](char ch) {
						segment.push_back(ch);
						return resolve(none<std::string>());
					});
				} else {
					return resolve(some<std::string>());
				}
			});
		}, std::string()));
	}

	//Reads including delim but does not include it in the return value
	future<std::string> read_until(istream& stream, char delim, const std::size_t max) {
		std::size_t nread = 0;
		return async_while<std::string>(capture([&stream, delim, nread, max](std::string& word) mutable {
			return stream.get().and_then<optional<std::string>>([&word, delim, &nread, max](optional<int> ch) {
				if (!ch)
					return resolve(some<std::string>(std::move(word)));
				nread += 1;
				if (ch == delim)
					return resolve(some<std::string>(std::move(word)));
				word.push_back(*ch);
				if (nread == max)
					return resolve(some<std::string>(std::move(word)));
				return resolve(none<std::string>());
			});
		}, std::string()));
	}


	future<request_uri> parse_request_uri_new(istream& stream) {
		return get_ch(stream).and_then<request_uri>([&stream](int ch) {
			if (ch == '/') {
				path p;
				p.push_back("/");

				std::size_t nread = 0;
				return async_while<request_uri>(capture([&stream, nread](path& p, std::string& segment) mutable {
					return get_ch(stream).and_then<optional<request_uri>>([&stream, &p, &segment, &nread](int ch) {
						if (nread += 1 > max_uri_length)
							throw http_error(http_status_code::uri_too_long);
						if (ch == '/') {
							p.push_back(segment);
							segment.clear();
							return resolve(none<request_uri>());
						} else if (ch == '%') {
							if (nread + 2 > max_uri_length || nread + 2 < nread)
								throw http_error(http_status_code::uri_too_long);
							nread += 2;
							return unescape(stream).and_then<optional<request_uri>>([&segment](int ch) {
								segment.push_back(ch);
								return resolve(none<request_uri>());
							});
						} else if (is_segment_char(ch)) {
							segment.push_back(ch);
							return resolve(none<request_uri>());
						} else if (ch == ' ') {
							return resolve(some<request_uri>(request_uri(std::move(p))));
						} else {
							throw http_error(http_status_code::bad_request);
						}
					});
				}, std::move(p), std::string()));
			} else {
				if (ch == ' ') {
					//empty authority
					return resolve(request_uri(std::string()));
				}
				return read_until(stream, ' ', max_uri_length).and_then<request_uri>([](std::string in) {
					return resolve(request_uri(std::move(in)));
				});
			}
		});
	}*/

	static unsigned int catoui(int ch) {
		return ch - '0';
	}

	static future<unsigned int> parse_status_code(istream& stream) {
		return get_ch(stream).and_then<unsigned int>([&stream](int first_ch) {
			if (!std::isdigit(first_ch))
				throw http_error(http_error_code::bad_status_digit);
			return get_ch(stream).and_then<unsigned int>([&stream, first_ch](int second_ch) {
				if (!std::isdigit(second_ch))
					throw http_error(http_error_code::bad_status_digit);
				return get_ch(stream).and_then<unsigned int>([first_ch, second_ch](int third_ch) {
					if (!std::isdigit(third_ch))
						throw http_error(http_error_code::bad_status_digit);
					return resolve(catoui(first_ch) * 100 + catoui(second_ch) * 10 + catoui(third_ch));
				});
			});
		});
	}

	static future<std::string> parse_reason_phrase(buffered_istream& stream) {
		return async_while<std::string>(capture([&stream](std::string& reason) {
			return stream.peek().and_then<optional<std::string>>([&stream, &reason](optional<int> ch) {
				if (!ch || is_crlf(*ch))
					return resolve(some<std::string>(std::move(reason)));
				reason.push_back(*ch);
				if (reason.size() > max_reason_phrase_length)
					throw http_error(http_error_code::reason_phrase_too_long);
				stream.get_now();
				return resolve(none<std::string>());
			});
		}, std::string()));
	}

	static future<std::string> parse_method(istream& stream) {
		return async_while<std::string>(capture([&stream](std::string &method) {
			return get_ch(stream).and_then<optional<std::string>>([&method](int ch) {
				if (ch == ' ') {
					if (method.empty())
						throw http_error(http_error_code::missing_method);
					return resolve(some<std::string>(std::move(method)));
				} else if (is_token(ch)) {
					method.push_back(ch);
					if (method.size() > max_method_length)
						throw http_error(http_error_code::method_too_long);
					return resolve(none<std::string>());
				} else {
					throw http_error(http_error_code::bad_method);
				}
			});
		}, std::string()));
	}

	future<std::string> parse_request_uri(istream& stream) {
		std::string request_uri;
		return async_while<std::string>(capture([&stream](std::string& request_uri) {
			return stream.get().and_then<optional<std::string>>([&request_uri](optional<int> ch) {
				if (!ch || ch == ' ')
					return resolve(some<std::string>(std::move(request_uri)));
				if (*ch < 32 || *ch > 126)
					throw http_error(http_error_code::bad_request_uri);
				request_uri.push_back(std::char_traits<char>::to_char_type(*ch));
				if (request_uri.length() > max_uri_length)
					throw http_error(http_error_code::request_uri_too_long);
				return resolve(none<std::string>());
			});
		}, std::move(request_uri)));
	}

	static future<unit> assert_string(istream &stream, std::string str) {
		return expect_string(stream, str).and_then<unit>([](bool b) {
			if (!b)
				throw http_error(http_error_code::bad_request);
			return resolve(unit());
		});
	}

	template <class UnsignedT>
	future<optional<UnsignedT>> parse_unsigned(buffered_istream& stream) {//TODO return read character to make it fully streamed
		optional<UnsignedT> result;
		return async_while<optional<UnsignedT>>(capture([&stream](optional<UnsignedT>& result) {
			return stream.peek().and_then<optional<optional<UnsignedT>>>([&stream, &result](optional<int> ch) {
				if (!ch || !std::isdigit(*ch))
					return resolve(some<optional<UnsignedT>>(std::move(result)));
				if (!result)
					result = some<UnsignedT>(UnsignedT());

				result = *result * 10 + *ch - '0';

				stream.get_now();
				return resolve(none<optional<UnsignedT>>());
			});
		}, std::move(result)));
	}

	future<http_version> parse_http_version(buffered_istream& stream) {
		return expect_string(stream, "HTTP/").and_then<http_version>([&stream](bool b) {
			if (!b)
				throw http_error(http_error_code::bad_version);
			return parse_unsigned<http_version::version_type>(stream).and_then<http_version>([&stream](optional<http_version::version_type> major_opt) {
				if (!major_opt)
					throw http_error(http_error_code::missing_major_version);
				http_version::version_type major = *major_opt;
				return assert_string(stream, ".").and_then<http_version>([&stream, major](unit)  {
					return parse_unsigned<http_version::version_type>(stream).and_then<http_version>([major](optional<http_version::version_type> minor_opt) {
						if (!minor_opt)
							throw http_error(http_error_code::missing_minor_version);
						return resolve(http_version(major, *minor_opt));
					});
				});
			});
		});
	}

	future<http_request> parse_request(buffered_istream& stream) {
		return parse_method(stream).and_then<http_request>([&stream](std::string method) {
			return parse_request_uri(stream).and_then<http_request>(capture([&stream](std::string& method , std::string uri) {
				return parse_http_version(stream).and_then<http_request>(capture([&stream](std::string& method, std::string& uri, http_version version) {
					return assert_string(stream, "\r\n").and_then<http_request>(capture([&stream, version](std::string& method, std::string& uri, unit) {
						return parse_headers(stream).and_then<http_request>(capture([version](std::string& method, std::string& uri, header_map headers) {
							return resolve(http_request(std::move(method), std::move(uri), version, std::move(headers)));
						}, std::move(method), std::move(uri)));
					}, std::move(method), std::move(uri)));
				}, std::move(method), std::move(uri)));
			}, std::move(method)));
		});
	}

	future<http_response> parse_response(buffered_istream& stream) {
		return parse_http_version(stream).and_then<http_response>([&stream](http_version version) {
			return parse_status_code(stream).and_then<http_response>([&stream, version](unsigned int status_code) {
				return parse_reason_phrase(stream).and_then<http_response>([&stream, version, status_code](std::string reason) {
					return assert_string(stream, "\r\n").and_then<http_response>(capture([&stream, version, status_code](std::string& reason, unit) {
						return parse_headers(stream).and_then<http_response>(capture([version, status_code](std::string& reason, header_map headers) {
							return resolve(http_response(version, status_code, std::move(reason), std::move(headers)));
						}, std::move(reason)));
					}, std::move(reason)));
				});
			});
		});
	}
}
