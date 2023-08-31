#ifndef COBRA_HTTP_UTIL_HH
#define COBRA_HTTP_UTIL_HH

#include <format>
#include <optional>
#include <string>

namespace cobra {
	std::string hexify(int i);
	std::optional<int> unhexify(char ch);
	std::optional<int> unhexify(std::optional<char> ch);

	template <std::predicate<char> Predicate>
	std::string hexify(std::string_view string, Predicate pred) {
		std::string result;

		for (char ch : string) {
			if (pred(ch)) {
				result.push_back(ch);
			} else {
				result.append(hexify(ch));
			}
		}

		return result;
	}

	bool is_alnum(char ch);
	bool is_unreserved(char ch);
	bool is_delim(char ch);
	bool is_uri_segment(char ch);
	bool is_uri_query(char ch);
	bool is_http_token(char ch);
	bool is_http_ws(char ch);
	bool is_http_ctl(char ch);
	bool is_http_uri(char ch);
	bool is_http_reason(char ch);
	bool is_cgi_value(char ch);
} // namespace cobra

#endif
