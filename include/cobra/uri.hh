#ifndef COBRA_URI_HH
#define COBRA_URI_HH

#include "cobra/asyncio/stream.hh"
#include "cobra/parse_utils.hh"

#include <cctype>
#include <string>
#include <variant>
#include <vector>

#include <iostream>

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
		inline bool is_reserved(int ch) {
			return ch == ';' || ch == '/' || ch == '?' || ch == ':' || ch == '@' || ch == '&' || ch == '=' ||
				   ch == '+' || ch == '$' || ch == ',';
		}
		inline bool is_unreserved(int ch) {
			return is_alpanum(ch) || is_mark(ch);
		}
		inline bool is_hex(int ch) {
			return is_digit(ch) || (ch >= 'A' && ch <= 'F') || (ch >= 'a' && ch <= 'f');
		}
		unsigned char hex_to_byte(int ch);

		class uri_scheme {
			std::string _uri_scheme;

			explicit uri_scheme(std::string uri_scheme);

		public:
			static constexpr std::size_t max_length = 128;
			uri_scheme() = delete;

			static task<uri_scheme> parse(buffered_istream_reference stream);

			inline const std::string& get() const {
				return _uri_scheme;
			}
		};

		template <AsyncInputStream Stream>
		class unescape_stream : public istream_impl<unescape_stream<Stream>> {
			Stream _stream;

		public:
			using char_type = typename Stream::char_type;
			using traits_type = typename Stream::traits_type;
			using int_type = typename Stream::int_type;
			using pos_type = typename Stream::pos_type;
			using off_type = typename Stream::off_type;

			unescape_stream(Stream stream) : _stream(std::move(stream)) {}

			task<std::size_t> read(char_type* data, std::size_t count) {
				std::size_t nwritten = 0;
				while (nwritten < count) {
					auto ch = co_await _stream.get();
					if (ch) {
						if (*ch == '%') {
							auto left = co_await _stream.get();
							auto right = co_await _stream.get();

							if (!left || !right || !is_hex(*left) || !is_hex(*right)) {
								throw parse_error("Invalid escape sequence");
							}
							data[nwritten] = hex_to_byte(*left) << 4 | hex_to_byte(*right);
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

			explicit path_word(std::string uri_scheme);

		public:
			static constexpr std::size_t max_length = 512;

			inline const std::string& get() const {
				return _word;
			}

			static task<path_word> parse(buffered_istream_reference stream);
		};

		class segment {
			path_word _path;
			std::vector<path_word> _params;

		public:
			static constexpr std::size_t max_params = 5;

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

			static task<segment> parse(buffered_istream_reference stream);
		};

		class path_segments {
			std::vector<segment> _segments;

			explicit path_segments(std::vector<segment> segments);

		public:
			inline const std::vector<segment>& segments() const {
				return _segments;
			}

			static task<path_segments> parse(buffered_istream_reference stream);
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

			static task<abs_path> parse(buffered_istream_reference stream);
		};

		class authority {
			std::string _authority;

			explicit authority(std::string auth);

		public:
			static constexpr std::size_t max_length = 256;

			authority() = delete;

			inline const std::string& get() const {
				return _authority;
			}

			static task<authority> parse(buffered_istream_reference stream);
		};

		class net_path {
			authority _authority;
			std::optional<abs_path> _path;

		public:
			net_path(authority auth, std::optional<abs_path> path = std::nullopt);

			inline authority& auth() {
				return _authority;
			}
			inline const authority& auth() const {
				return _authority;
			}
			inline std::optional<abs_path>& path() {
				return _path;
			}
			inline const std::optional<abs_path>& path() const {
				return _path;
			}

			static task<net_path> parse(buffered_istream_reference stream);
		};

		class uri_query {
			std::string _uri_query;

			explicit uri_query(std::string str);

		public:
			static constexpr std::size_t max_length = 1024;

			inline const std::string& get() const {
				return _uri_query;
			}

			static task<uri_query> parse(buffered_istream_reference stream);
		};

		class hier_part {
			std::variant<net_path, abs_path> _path;
			std::optional<uri_query> _query;

		public:
			hier_part(std::variant<net_path, abs_path> path, std::optional<uri_query> query = std::nullopt);

			inline std::variant<net_path, abs_path>& path() {
				return _path;
			};
			inline const std::variant<net_path, abs_path>& path() const {
				return _path;
			};
			inline std::optional<uri_query>& query() {
				return _query;
			}
			inline const std::optional<uri_query>& query() const {
				return _query;
			}

			static task<hier_part> parse(buffered_istream_reference stream);
		};

		class opaque_part {
			std::string _opaque;

			explicit opaque_part(std::string opaque);

		public:
			static constexpr std::size_t max_length = 1024;

			opaque_part() = delete;

			inline const std::string& get() const {
				return _opaque;
			};

			static task<opaque_part> parse(buffered_istream_reference stream);
		};

		class abs_uri {
			uri_scheme _uri_scheme;
			std::variant<hier_part, opaque_part> _part;

		public:
			abs_uri(uri_scheme scheme, std::variant<hier_part, opaque_part> part);

			inline uri_scheme& scheme() {
				return _uri_scheme;
			}
			inline const uri_scheme& scheme() const {
				return _uri_scheme;
			}
			inline std::variant<hier_part, opaque_part>& part() {
				return _part;
			}
			inline const std::variant<hier_part, opaque_part>& part() const {
				return _part;
			}

			static task<abs_uri> parse(buffered_istream_reference stream);
		};
	} // namespace uri
} // namespace cobra

#endif
