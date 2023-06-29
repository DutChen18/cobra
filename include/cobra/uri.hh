#ifndef COBRA_URI_HH
#define COBRA_URI_HH

#include "cobra/asyncio/stream.hh"

#include <string>
#include <cctype>

namespace cobra {

	namespace uri {
		inline bool is_alpha(int ch) { return std::isalpha(ch); }
		inline bool is_alpanum(int ch) { return std::isalnum(ch); }
		inline bool is_digit(int ch) { return std::isdigit(ch); }
		inline bool is_mark(int ch) {
			return ch == '-' || ch == '_' || ch == '.' || ch == '!' || ch == '~' || ch == '*' || ch == '\'' ||
                      ch == '(' || ch == ')';
		}
		inline bool is_unreserved(int ch) { return is_alpanum(ch) || is_mark(ch); }

		class scheme {
			std::string _scheme;

			explicit scheme(std::string scheme);
		public:
			static constexpr std::size_t max_length = 128;
			scheme() = delete;

			static task<scheme> parse(buffered_istream &stream);

			inline const std::string& get() const { return _scheme; }
		};

		class segment {
			std::string _segment;

			explicit segment(std::string scheme);
		public:
			static constexpr std::size_t max_length = 512;
			segment() = delete;
			static task<segment> parse(buffered_istream &stream);
		};

	}
}

#endif
