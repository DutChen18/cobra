#ifndef COBRA_URI_HH
#define COBRA_URI_HH

#include "cobra/asyncio/stream.hh"
#include "cobra/parse_utils.hh"

#include <cctype>
#include <string>
#include <vector>

namespace cobra {

	namespace uri {
		inline bool is_alpha(int ch) {
			return std::isalpha(ch);
		}
		inline bool is_alpanum(int ch) {
			return std::isalnum(ch);
		}
		inline bool is_digit(int ch) {
			return std::isdigit(ch);
		}
		inline bool is_mark(int ch) {
			return ch == '-' || ch == '_' || ch == '.' || ch == '!' || ch == '~' || ch == '*' || ch == '\'' ||
				   ch == '(' || ch == ')';
		}
		inline bool is_unreserved(int ch) {
			return is_alpanum(ch) || is_mark(ch);
		}
		inline bool is_hex(int ch) {
			return is_digit(ch) || (ch >= 'A' && ch <= 'F') || (ch >= 'a' && ch <= 'f');
		}
		unsigned char hex_to_byte(int ch);

		class scheme {
			std::string _scheme;

			explicit scheme(std::string scheme);

		public:
			static constexpr std::size_t max_length = 128;
			scheme() = delete;

			static task<scheme> parse(buffered_istream& stream);

			inline const std::string& get() const {
				return _scheme;
			}
		};

		template <AsyncReadableStream Stream>
		class unescape_stream : public istream {
			Stream _stream;

		public:
			using char_type = typename Stream::char_type;
			using traits_type = typename Stream::traits_type;
			using int_type = typename Stream::int_type;
			using pos_type = typename Stream::pos_type;
			using off_type = typename Stream::off_type;

			constexpr unescape_stream(Stream stream) : _stream(std::move(stream)) {}

			[[nodiscard]] task<std::size_t> read(char_type* data, std::size_t count) override {
				std::size_t nwritten = 0;
				for (; nwritten < count; ++nwritten) {
					auto ch = co_await _stream.get();
					if (ch) {
						if (*ch == '%') {
							auto left = co_await _stream.get();
							auto right = co_await _stream.get();

							if (!left || !right || !is_hex(*left) || !is_hex(*right)) {
								throw parse_error("Invalid escape sequence");
							}
							data[nwritten] = hex_to_byte(*left) << 16 | hex_to_byte(*right);
						} else {
							data[nwritten] = *ch;
						}
						++nwritten;
					} else {
						break;
					}
				}
				co_return nwritten;
			}
		};

		class path_word {
			std::string _word;

			explicit path_word(std::string scheme);

		public:
			static constexpr std::size_t max_length = 512;

			path_word();
			path_word(const path_word& other) = default;
			path_word(path_word&& other) = default;

			inline const std::string& get() const {
				return _word;
			}

			static task<path_word> parse(buffered_istream& stream);
		};

		class segment {
			path_word _path;
			std::vector<path_word> _params;

		public:
			static constexpr std::size_t max_params = 5;

			segment();
			segment(path_word path, std::vector<path_word> params = std::vector<path_word>());

			inline path_word& path() {
				return _path;
			}
			inline const path_word& path() const {
				return _path;
			}
			inline std::vector<path_word>& params() {
				return _params;
			}
			inline const std::vector<path_word>& params() const {
				return _params;
			}

			static task<segment> parse(buffered_istream& stream);
		};

		class path_segments {
			std::vector<segment> _segments;

			explicit path_segments(std::vector<segment> segments);

		public:
			path_segments() = delete;
			path_segments(const path_segments& other) = default;
			path_segments(path_segments&& other) = default;

			inline const std::vector<segment>& segments() const {
				return _segments;
			}

			static task<path_segments> parse(buffered_istream& stream);
		};

		class abs_path {
			path_segments _segments;

		public:
			explicit abs_path(path_segments segments);

			inline path_segments& segments() {
				return _segments;
			}
			inline const path_segments& segments() const {
				return _segments;
			}

			static task<abs_path> parse(buffered_istream& stream);
		};
	} // namespace uri
} // namespace cobra

#endif
