#include "cobra/http.hh"
#include "cobra/future.hh"
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

	future<http_method> parse_method(istream& stream) {
		std::string str;
		return async_while<http_method>(capture([&stream](std::string& str) {
			return stream.get().and_then<optional<http_method>>([&str](optional<int> ch) {
				if (!ch || ch == ' ') {
					http_method method = parse_method(str);

					if (method == http_method::unknown)
						throw http_error(http_status_code::not_implemented);
					else
						return resolve(some<http_method>(method));
				}
				str.push_back(*ch);

				if (str.length() > 16)
					throw http_error(http_status_code::not_implemented);
				return resolve(none<http_method>());
			});
		}, std::move(str)));
	}

	/*
	bool is_mark(int ch) {
		return ch == '-' || ch == '_' || ch == '.' || ch == '!' || ch == '~' || ch == '*' || ch == '\'' ||
                      ch == '(' || ch == ')';
	}

	bool is_unreserved(int ch) {
		return std::isalnum(ch) || is_mark(ch);
	}

	static bool is_pchar(int ch) {
		return is_unreserved(ch) ||
		ch == ':' || ch == '@' || ch == '&' || ch == '=' || ch == '+' || ch == '$' || ch == ',';
	}

	future<path> parse_abs_path(buffered_istream& stream) {
		path res;
		std::string buf;
		return async_while<path>(capture([&stream](path& res, std::string& buf) {
			stream.peek().and_then<path>([&stream, &res, &buf](optional<int> ch) {
				if (!ch) {
				}
			});
		}, std::move(res), std::move(buf)));
	}*/

	future<std::string> get_read_until(buffered_istream& stream, char delim) {
		std::string word;
		return async_while<std::string>(capture([&stream, delim](std::string& word) {
			return stream.peek().and_then<optional<std::string>>([&stream, &word, delim](optional<int> ch) {
				if (!ch)
					return resolve(some<std::string>(std::move(word)));
				if (ch == delim)
					return resolve(some<std::string>(std::move(word)));
				return stream.ignore(1).and_then<optional<std::string>>([](std::size_t) {
					return resolve(none<std::string>());
				});
			});
		}, std::move(word)));
	}

	/*
	future<std::string> parse_uri(buffered_istream& stream) {
		return async_while<std::string>([&stream]() {
			stream.peek().and_then<std::string>([&stream](optional<int> ch) {
				if (!ch)
					throw new http_error(http_status_code::bad_request);
				if (ch == '/')
					return parse_abs_path(stream);
			});
		});
	}*/

	future<std::string> parse_request_uri(istream& stream) {
		std::string request_uri;
		return async_while<std::string>(capture([&stream](std::string& request_uri) {
			return stream.get().and_then<optional<std::string>>([&request_uri](optional<int> ch) {
				if (!ch || ch == ' ')
					return resolve(some<std::string>(std::move(request_uri)));
				request_uri.push_back(*ch);
				if (request_uri.length() > 1024)
					throw http_error(http_status_code::uri_too_long);
				return resolve(none<std::string>());
			});
		}, std::move(request_uri)));
	}

	future<bool> expect_string(istream& stream, std::string str) {
		std::string::size_type offset = 0;
		return async_while<bool>(capture([&stream, offset](std::string& str) mutable {
			if (offset == str.size())
				return resolve(some<bool>(true));
			return stream.get().and_then<optional<bool>>([&str, &offset](optional<int> ch) {
				if (ch != str[offset])
					return resolve(some<bool>(false));
				offset += 1;
				return resolve(none<bool>());
			});
		}, std::move(str)));
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

				UnsignedT tmp = *result * 10 + '0' - *ch;
				if (tmp < *result)
					return resolve(some<optional<UnsignedT>>(std::move(result)));
				result = tmp;
				
				return stream.ignore(1).and_then<optional<optional<UnsignedT>>>([](std::size_t) {
					return resolve(none<optional<UnsignedT>>());
				});
			});
		}, std::move(result)));
	}

	future<http_version> parse_http_version(buffered_istream& stream) {
		return expect_string(stream, "HTTP/").and_then<http_version>([&stream](bool b) {
			if (!b)
				throw http_error(http_status_code::bad_request);
			return parse_unsigned<http_version::version_type>(stream).and_then<http_version>([&stream](optional<http_version::version_type> major_opt) {
				if (!major_opt)
					throw http_error(http_status_code::bad_request);
				http_version::version_type major = *major_opt;
				return expect_string(stream, ".").and_then<http_version>([&stream, major](bool b)  {
					if (!b)
						throw http_error(http_status_code::bad_request);
					return parse_unsigned<http_version::version_type>(stream).and_then<http_version>([major](optional<http_version::version_type> minor_opt) {
						if (!minor_opt)
							throw http_error(http_status_code::bad_request);
						return resolve(http_version(major, *minor_opt));
					});
				});
			});
		});
	}

	future<http_request> parse_request(buffered_istream &stream) {
		return parse_method(stream).and_then<http_request>([&stream](http_method method) {
			return parse_request_uri(stream).and_then<http_request>([&stream, method](std::string uri) {
				return parse_http_version(stream).and_then<http_request>([&stream, method, uri](http_version version) { //TODO move string instead of copy
					return expect_string(stream, "\r\n").and_then<http_request>([&stream, method, uri, version](bool b) {
						if (!b)
							throw http_error(http_status_code::bad_request);
						return parse_headers(stream).and_then<http_request>([method, uri, version](header_map map) {
							return resolve(http_request(method, uri, version, map));
						});
					});
				});
			});
		});
	}

	http_request::http_request(http_method method, const std::string& request_uri, http_version version, const header_map& map) : _method(method), _request_uri(request_uri), _version(version), _headers(map) {}

	/*

	future<http_version> parse_http_version(std::shared_ptr<istream> stream) {
		std::shared_ptr<http_version> version(new http_version());
		return cobra::async_while([stream, version]() {
		}).then<http_version>([version]() {
			return *version;
		});
	}*/

}
