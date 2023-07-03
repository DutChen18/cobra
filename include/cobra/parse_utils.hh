#ifndef COBRA_PARSE_UTILS_HH
#define COBRA_PARSE_UTILS_HH

#include "cobra/asyncio/stream.hh"
#include "cobra/asyncio/stream_utils.hh"
#include "cobra/exception.hh"

#include <optional>
#include <stdexcept>
#include <string>
#include <concepts>

namespace cobra {

	template <class Stream>
	task<typename Stream::int_type> assert_get(Stream& stream) {
		auto ch = co_await stream.get();
		if (ch) {
			co_return *ch;
		} else {
			throw parse_error("Unexpected EOF");
		}
	}

	template <AsyncReadableStream Stream>
	task<typename Stream::int_type> expect(Stream& stream) {
		auto ch = co_await stream.get();
		if (ch) {
			co_return *ch;
		} else {
			throw parse_error("Unexpected EOF");
		}
	}

	template <AsyncPeekableStream  Stream>
	task<typename Stream::int_type> expect_peek(Stream& stream) {
		auto ch = co_await stream.peek();
		if (ch) {
			co_return *ch;
		} else {
			throw parse_error("Unexpected EOF");
		}
	}

	template <AsyncReadableStream Stream, class UnaryPredicate>
	task<void> expect(Stream& stream,  UnaryPredicate p) requires std::predicate<UnaryPredicate, typename Stream::int_type> {
		auto ch = co_await expect(stream);
		if (!p(ch)) {
			throw parse_error("Unexpected char");
		}
	}

	template <AsyncReadableStream Stream, class String>
	task<void> expect(Stream& stream, const String& str) requires requires(String str, std::size_t idx) {
		str[idx];
	}
	{
		for (auto&& expected_ch : str) {
			auto ch = co_await stream.get();
			if (!ch) {
				throw parse_error("Unexpected EOF");
			} else if (ch != expected_ch) {
				throw parse_error("Unexpected char");
			}
		}
	}

	template <AsyncReadableStream Stream>
	task<void> expect(Stream& stream, typename Stream::int_type expected_ch) {
		auto ch = co_await stream.get();
		if (!ch) {
			throw parse_error("Unexpected EOF");
		} else if (ch != expected_ch) {
			throw parse_error("Unexpected char");
		}
	}
} // namespace cobra

#endif
