#include "cobra/http.hh"
#include "cobra/future.hh"

namespace cobra {
	static header_map::key_type normalize_key(header_map::key_type key) {
		bool was_alpha = false;

		for (char& ch : key) {
			if (was_alpha) {
				ch = std::tolower(ch);
			} else {
				ch = std::toupper(ch);
			}

			was_alpha = std::isalpha(ch);
		}

		return key;
	}

	header_map::mapped_type& header_map::insert_or_assign(const key_type& key, const mapped_type& value) {
		std::pair<map_type::iterator, bool> result = map.emplace(std::make_pair(normalize_key(key), value));

		if (!result.second) {
			result.first->second = value;
		}

		return result.first->second;
	}

	header_map::mapped_type& header_map::insert_or_append(const key_type& key, const mapped_type& value) {
		std::pair<map_type::iterator, bool> result = map.emplace(std::make_pair(normalize_key(key), value));

		if (!result.second) {
			result.first->second.push_back(' ');
			result.first->second += value;
		}

		return result.first->second;
	}

	const header_map::mapped_type& header_map::at(const key_type& key) const {
		return map.at(normalize_key(key));
	}

	bool header_map::contains(const key_type& key) const {
		return map.find(normalize_key(key)) != map.end();
	}

	header_map::size_type header_map::size() const {
		return map.size();
	}

	header_map::iterator header_map::begin() {
		return map.begin();
	}

	header_map::const_iterator header_map::begin() const {
		return map.begin();
	}

	header_map::iterator header_map::end() {
		return map.end();
	}

	header_map::const_iterator header_map::end() const {
		return map.end();
	}

	static future<std::string> parse_key(istream& stream, int first_ch) {
		return async_while<std::string>(capture([&stream](std::string& key) {
			return get_ch(stream).and_then<optional<std::string>>([&key](int ch) {
				if (ch == ':') {
					return resolve(some<std::string>(std::move(key)));
				} else if (is_token(ch)) {
					key.push_back(std::char_traits<char>::to_char_type(ch));

					if (key.length() > max_header_key_length) {
						throw http_error(http_error_code::header_key_too_long);
					} else {
						return resolve(none<std::string>());
					}
				} else {
					throw http_error(http_error_code::bad_header_key);
				}
			});
		}, std::string(1, first_ch)));
	}

	static future<std::string> parse_value(istream& stream, int& first_ch) {
		return async_while<std::string>(capture([&stream, &first_ch](std::string& value, bool& had_ws) {
			return get_ch(stream).and_then<optional<std::string>>([&stream, &value, &first_ch, &had_ws](int ch) {
				if (ch == '\r') {
					return get_ch(stream).and_then<int>([&stream](int ch) {
						if (ch != '\n') {
							throw http_error(http_error_code::missing_lf_after_cr);
						} else {
							return get_ch(stream);
						}
					}).and_then<optional<std::string>>([&value, &first_ch, &had_ws](int ch) {
						if (ch == ' ' || ch == '\t') {
							had_ws = !value.empty();
							return resolve(none<std::string>());
						} else {
							first_ch = ch;
							return resolve(some<std::string>(std::move(value)));
						}
					});
				} else if (ch == ' ' || ch == '\t') {
					had_ws = !value.empty();
					return resolve(none<std::string>());
				} else if (!is_ctl(ch)) {
					if (had_ws)
						value.push_back(' ');
					had_ws = false;
					value.push_back(std::char_traits<char>::to_char_type(ch));

					if (value.length() > max_header_value_length) {
						throw http_error(http_error_code::header_value_too_long);
					} else {
						return resolve(none<std::string>());
					}
				} else if (ch == '\n') {
					throw http_error(http_error_code::missing_cr_before_lf);
				} else {
					throw http_error(http_error_code::bad_header_value);
				}
			});
		}, std::string(), false));
	}
	
	future<header_map> parse_headers(istream& stream) {
		return get_ch(stream).and_then<header_map>([&stream](int first_ch) {
			return async_while<header_map>(capture([&stream](header_map& headers, int& first_ch) {
				if (first_ch == '\r') {
					return get_ch(stream).and_then<optional<header_map>>([&headers](int ch) {
						if (ch != '\n') {
							throw http_error(http_error_code::missing_lf_after_cr);
						} else {
							return resolve(some<header_map>(std::move(headers)));
						}
					});
				} else if (is_token(first_ch)) {
					return parse_key(stream, first_ch).and_then<optional<header_map>>([&stream, &headers, &first_ch](std::string key) {
						return parse_value(stream, first_ch).and_then<optional<header_map>>(capture([&headers](std::string& key, std::string value) {
							std::string& tmp = headers.insert_or_append(std::move(key), std::move(value));

							if (tmp.length() > max_header_value_length || headers.size() > max_header_count) {
								throw http_error(http_error_code::header_value_too_long);
							} else {
								return resolve(none<header_map>());
							}
						}, std::move(key)));
					});
				} else if (first_ch == '\n') {
					throw http_error(http_error_code::missing_cr_before_lf);
				} else {
					throw http_error(http_error_code::bad_request);
				}
			}, header_map(), std::move(first_ch)));
		});
	}

	static future<unit> consume_optional_newline(buffered_istream& stream) {
		return stream.peek().and_then<unit>([&stream](optional<int> ch) {
			if (ch == '\n') {
				return stream.get().and_then<unit>([](optional<int>) {
					return resolve(unit());
				});
			} else {
				return resolve(unit());
			}
		});
	}

	static future<std::string> parse_cgi_value(buffered_istream& stream) {
		return async_while<std::string>(capture([&stream](std::string& value) {
			return get_ch(stream).and_then<optional<std::string>>([&stream, &value](int ch) {
				if (ch == '\r') {
					return consume_optional_newline(stream).and_then<optional<std::string>>([&value](unit) {
						return resolve(some<std::string>(std::move(value)));
					});
				} else if (ch == '\n') {
					return resolve(some<std::string>(std::move(value)));
				} else if (is_cgi_value(ch)) {
					value.push_back(std::char_traits<char>::to_char_type(ch));

					if (value.length() > max_header_value_length) {
						throw http_error(http_error_code::header_value_too_long);
					} else {
						return resolve(none<std::string>());
					}
				} else {
					throw http_error(http_error_code::bad_header_value);
				}
			});
		}, std::string()));
	}

	future<header_map> parse_cgi_headers(buffered_istream& stream) {
		return async_while<header_map>(capture([&stream](header_map& headers) {
			return get_ch(stream).and_then<optional<header_map>>([&stream, &headers](int ch) {
				if (ch == '\r') {
					return consume_optional_newline(stream).and_then<optional<header_map>>([&headers](unit) {
						return resolve(some<header_map>(std::move(headers)));
					});
				} else if (ch == '\n') {
					return resolve(some<header_map>(std::move(headers)));
				} else if (is_token(ch)) {
					return parse_key(stream, ch).and_then<optional<header_map>>([&stream, &headers](std::string key) {
						return parse_cgi_value(stream).and_then<optional<header_map>>(capture([&headers](std::string& key, std::string value) {
							std::string& tmp = headers.insert_or_append(std::move(key), std::move(value));

							if (tmp.length() > max_header_value_length || headers.size() > max_header_count) {
								throw http_error(http_error_code::header_value_too_long);
							} else {
								return resolve(none<header_map>());
							}
						}, std::move(key)));
					});
				} else {
					throw http_error(http_error_code::bad_request);
				}
			});
		}, header_map()));
	}
	
	std::ostream& operator<<(std::ostream& os, const header_map& headers) {
		for (const auto& pair : headers) {
			os << normalize_key(pair.first) << ": " << pair.second << "\r\n";
		}

		return os << "\r\n";
	}
}
