#ifndef COBRA_ASIO_UTILS_HH
#define COBRA_ASIO_UTILS_HH

#include "cobra/future.hh"
#include "cobra/asio.hh"
#include "cobra/function.hh"
#include <string>

namespace cobra {

	template <class CharT, class Traits = std::char_traits<CharT>, class Allocator = std::allocator<CharT>>
	future<bool> expect_string(basic_istream<CharT, Traits>& stream, std::basic_string<CharT, Traits, Allocator> str) {
		std::size_t offset = 0;
		return async_while<bool>(capture([&stream, offset](const std::basic_string<CharT, Traits, Allocator>& str) mutable {
			if (offset == str.size())
				return resolve(some<bool>(true));
			return stream.get().template and_then<optional<bool>>([&str, &offset](optional<int> ch) {
				if (ch != str[offset])
					resolve(some<bool>(false));
				offset += 1;
				return resolve(none<bool>());
			});
		}, std::move(str)));
	}

	template <class UnaryPredicate, class CharT, class Traits = std::char_traits<CharT>, class Allocator = std::allocator<CharT>>
	future<std::size_t> ignore_while(basic_buffered_istream<CharT, Traits, Allocator>& stream, UnaryPredicate p) {
		std::size_t nread = 0;
		return async_while<std::size_t>(capture([&stream, nread](UnaryPredicate& p) mutable {
			return stream.peek().template and_then<optional<std::size_t>>([&stream, &nread, &p](optional<int> ch) {
				if (!ch || !p(*ch))
					return resolve(some<std::size_t>(nread));
				stream.get_now();
				return resolve(none<std::size_t>());
			});
		}, std::move(p)));
	}
}
#endif
