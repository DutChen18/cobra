#include "cobra/http.hh"

#include "cobra/asyncio/stream_utils.hh"
#include "cobra/asyncio/stream.hh"
#include "cobra/parse_utils.hh"

#include <cctype>
#include <algorithm>
#include <optional>

namespace cobra {

	bool is_separator(int ch) {
		return ch == '(' || ch == ')' || ch == '<' || ch == '>' || ch == '@'
                      || ch == ',' || ch == ';' || ch == ':' || ch == '\\' || ch == '\''
                      || ch == '/' || ch == '[' || ch == ']' || ch == '?' || ch == '='
                      || ch == '{' || ch == '}' || ch == ' ' || ch == '\t';
	}

	bool is_ctl(int ch) {
		return std::iscntrl(ch);
	}

	http_token::http_token(std::string token) : _token(std::move(token)) {}

	http_token parse(const std::string& str) {
		if (str.empty()) {
			throw parse_error("Token too short");
		}
		if (str.length() > http_token::max_length) {
			throw parse_error("Token too long");
		}
		if (std::any_of(str.begin(), str.end(), [](char ch) { return is_separator(static_cast<int>(ch)) || is_ctl(static_cast<int>(ch)); })) {
			throw parse_error("Token contained invalid characters");
		}
		return http_token(str);
	}

	task<http_token> parse(buffered_istream& stream) {
		//http_token(co_await make_adapter(wrap_stream(stream)).take_while([](int ch) { return !is_separator(ch) && !is_ctl(ch); }).take(http_token::max_length).collect());
		co_return http_token(co_await make_adapter(wrap_stream(stream)).take_while([](int) { return true; }).take(http_token::max_length).collect());
	}
}
