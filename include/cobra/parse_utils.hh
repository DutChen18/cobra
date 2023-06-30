#ifndef COBRA_PARSE_UTILS_HH
#define COBRA_PARSE_UTILS_HH

#include "cobra/asyncio/stream.hh"
#include "cobra/exception.hh"

#include <optional>
#include <stdexcept>
#include <string>

namespace cobra {

	template <class Stream>
	[[nodiscard]] task<typename Stream::int_type> assert_get(Stream& stream) {
		auto ch = co_await stream.get();
		if (ch) {
			co_return *ch;
		} else {
			throw parse_error("Unexpected EOF");
		}
	}

	template <class Stream>
	[[nodiscard]] task<void> expect(Stream& stream, typename Stream::int_type expected_ch) {
		auto ch = co_await stream.get();
		if (!ch) {
			throw parse_error("Unexpected EOF");
		} else if (ch && ch != expected_ch) {
			throw parse_error("Unexpected char");
		}
	}
} // namespace cobra

#endif
