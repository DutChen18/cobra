#include "cobra/uri.hh"

#include "cobra/asyncio/stream_utils.hh"
#include "cobra/parse_utils.hh"

#include <cctype>

namespace cobra {

	namespace uri {
		scheme::scheme(std::string scheme) : _scheme(std::move(scheme)) {}

		task<scheme> parse(buffered_istream& stream) {
			if (is_alpha(co_await assert_get(stream))) {
				co_return scheme(co_await wrap_adapter(stream)
									 .take_while([](int ch) {
										 return is_alpha(ch) || is_digit(ch) || ch == '+' || ch == '-' || ch == '.';
									 })
									 .take(scheme::max_length)
									 .collect());
			} else {
				throw parse_error("An url must start with an alpha character");
			}
		}
	} // namespace uri
} // namespace cobra
