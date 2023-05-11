#include "cobra/http.hh"
#include "cobra/future.hh"

namespace cobra {
	void header_map::insert(std::string key, std::string value) {
		std::transform(key.begin(), key.end(), key.begin(), ::tolower);
		
		map_type::iterator iter = map.find(key);

		if (iter == map.end()) {
			map.insert(std::make_pair(key, std::move(value)));
		} else {
			iter->second = std::move(value);
		}
	}

	const std::string& header_map::at(std::string key) const {
		std::transform(key.begin(), key.end(), key.begin(), ::tolower);

		return map.at(key);
	}

	static bool is_ctl(int ch) {
		return (ch >= 0 && ch <= 31) || ch == 127;
	}

	static bool is_separator(int ch) {
		switch (ch) {
		case '(':
			return true;
		case ')':
			return true;
		case '<':
			return true;
		case '>':
			return true;
		case '@':
			return true;
		case ',':
			return true;
		case ';':
			return true;
		case ':':
			return true;
		case '\\':
			return true;
		case '"':
			return true;
		case '/':
			return true;
		case '[':
			return true;
		case ']':
			return true;
		case '?':
			return true;
		case '=':
			return true;
		case '{':
			return true;
		case '}':
			return true;
		case ' ':
			return true;
		case '\t':
			return true;
		default:
			return false;
		}
	}

	static bool is_token(int ch) {
		return !is_ctl(ch) && !is_separator(ch);
	}
	
	future<header_map> parse_headers(istream& stream) {
		return stream.get().and_then<header_map>([&stream](optional<int> first_ch) {
			return async_while<header_map>(capture([&stream](header_map& headers, optional<int>& first_ch) {
				if (!first_ch) {
					throw http_error(http_status_code::bad_request);
				} else if (*first_ch == '\r') {
					return stream.get().and_then<optional<header_map>>([&headers](optional<int> ch) {
						if (!ch || *ch != '\n') {
							throw http_error(http_status_code::bad_request);
						} else {
							return resolve(some<header_map>(std::move(headers)));
						}
					});
				} else if (is_token(*first_ch)) {
					return async_while<std::string>(capture([&stream](std::string& key) {
						return stream.get().and_then<optional<std::string>>([&key](optional<int> ch) {
							if (!ch) {
								throw http_error(http_status_code::bad_request);
							} else if (*ch == ':') {
								return resolve(some<std::string>(std::move(key)));
							} else if (is_token(*ch)) {
								key += *ch;
								return resolve(none<std::string>());
							} else {
								throw http_error(http_status_code::bad_request);
							}
						});
					}, std::string(1, *first_ch))).and_then<optional<header_map>>([&stream, &headers, &first_ch](std::string key) {
						return async_while<std::string>(capture([&stream, &first_ch](std::string& value, bool& had_ws) {
							return stream.get().and_then<optional<std::string>>([&stream, &value, &first_ch, &had_ws](optional<int> ch) {
								if (!ch) {
									throw http_error(http_status_code::bad_request);
								} else if (*ch == '\r') {
									return stream.get().and_then<optional<std::string>>([&stream, &value, &first_ch, &had_ws](optional<int> ch) {
										if (!ch || *ch != '\n') {
											throw http_error(http_status_code::bad_request);
										} else {
											return stream.get().and_then<optional<std::string>>([&value, &first_ch, &had_ws](optional<int> ch) {
												if (ch && (*ch == ' ' || *ch == '\t')) {
													had_ws = !value.empty();
													return resolve(none<std::string>());
												} else {
													first_ch = ch;
													return resolve(some<std::string>(std::move(value)));
												}
											});
										}
									});
								} else if (*ch == ' ' || *ch == '\t') {
									had_ws = !value.empty();
									return resolve(none<std::string>());
								} else if (!is_ctl(*ch)) {
									if (had_ws)
										value += ' ';
									had_ws = false;
									value += *ch;
									return resolve(none<std::string>());
								} else {
									throw http_error(http_status_code::bad_request);
								}
							});
						}, std::string(), false)).and_then<optional<header_map>>(capture([&headers](std::string& key, std::string value) {
							headers.insert(std::move(key), std::move(value));
							return resolve(none<header_map>());
						}, std::move(key)));
					});
				} else {
					throw http_error(http_status_code::bad_request);
				}
			}, header_map(), std::move(first_ch)));
		});
	}
}
