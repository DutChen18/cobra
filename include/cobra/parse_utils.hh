#ifndef COBRA_PARSE_UTILS_HH
#define COBRA_PARSE_UTILS_HH

#include <optional>
#include <stdexcept>
#include <string>
#include "cobra/asyncio/stream.hh"

namespace cobra {

	class parse_error : public std::runtime_error {
	public:
		parse_error(const std::string& what);
		parse_error(const char *);
	};

	task<int> assert_get(buffered_istream& stream);

	template<class Stream>
	task<typename Stream::int_type> assert_get(Stream& stream) {
		auto ch = stream.get();
		if (ch) {
			co_return *ch;
		} else {
			throw parse_error("Unexpected EOF");
		}
	}

	/*
	template<class Stream, class Predicate, class String = std::basic_string<typename Stream::char_type, typename Stream::traits_type>>
	task<String> take_while(Stream& stream, typename String::size_type max, Predicate p) {
		String result;
		while (result.length() <= max) {
			auto ch = stream.get();
			if (ch && p(*ch)) {
				result.push(ch);
			} else {
				break;
			}
		}
		return result;
	}*/

}

#endif
