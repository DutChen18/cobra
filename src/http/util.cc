#include "cobra/http/util.hh"

namespace cobra {
	std::string hexify(int i) {
		const char* charset = "0123456789ABCDEF";
		return std::format("%{}{}", charset[i >> 4 & 15], charset[i & 15]);
	}

	std::optional<int> unhexify(char ch) {
		if (ch >= '0' && ch <= '9') {
			return ch - '0';
		} else if (ch >= 'a' && ch <= 'f') {
			return ch - 'a' + 10;
		} else if (ch >= 'A' && ch <= 'F') {
			return ch - 'A' + 10;
		} else {
			return std::nullopt;
		}
	}

	bool is_alnum(char ch) {
		return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
	}

	bool is_unreserved(char ch) {
		return is_alnum(ch) || ch == '-' || ch == '.' || ch == '_' || ch == '~';
	}

	bool is_delim(char ch) {
		return ch == '!' || ch == '$' || ch == '&' || ch == '\'' || ch == '*' || ch == '+';
	}

	bool is_uri_segment(char ch) {
		return is_unreserved(ch) || is_delim(ch) || ch == '@' || ch == ',' || ch == '(' || ch == ')' || ch == ':' || ch == ';' || ch == '=';
	}

	bool is_uri_query(char ch) {
		return is_uri_segment(ch) || ch == '/' || ch == '?' || ch == '%';
	}

	bool is_http_token(char ch) {
		return is_unreserved(ch) || is_delim(ch) || ch == '#' || ch == '%' || ch == '^' || ch == '`' || ch == '|';
	}

	bool is_http_ws(char ch) {
		return ch == ' ' || ch == '\t';
	}

	bool is_http_ctl(char ch) {
		return (ch >= 0 && ch < 32) || ch == 127;
	}

	bool is_http_uri(char ch) {
		return is_uri_query(ch);
	}

	bool is_http_reason(char ch) {
		return is_http_ws(ch) || !is_http_ctl(ch);
	}

	bool is_cgi_value(char ch) {
		return (!is_http_ctl(ch) && ch >= 0 && ch <= 127) || ch == '\t';
	}
}
