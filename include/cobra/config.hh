#ifndef COBRA_CONFIG_HH
#define COBRA_CONFIG_HH

#include "cobra/print.hh"

#include <algorithm>
#include <any>
#include <cctype>
#include <compare>
#include <exception>
#include <filesystem>
#include <format>
#include <iostream>
#include <istream>
#include <limits>
#include <map>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <charconv>
#include <string_view>

#define COBRA_BLOCK_KEYWORDS                                                                                           \
	X(max_body_size)                                                                                                   \
	X(location)                                                                                                        \
	X(index)                                                                                                           \
	X(root)

#define COBRA_SERVER_KEYWORDS                                                                                          \
	X(listen)                                                                                                          \
	X(ssl)                                                                                                             \
	X(server_name)                                                                                                     \
	COBRA_BLOCK_KEYWORDS

namespace cobra {
	namespace fs = std::filesystem;
	using port = unsigned short;

	namespace config {

		struct buf_pos {
			std::size_t line;
			std::size_t col;
			std::size_t length;

			buf_pos() = delete;
			constexpr buf_pos(std::size_t line, std::size_t col) noexcept;
			constexpr buf_pos(std::size_t line, std::size_t col, std::size_t length) noexcept;
		};

		struct file_part : public buf_pos {
			std::optional<fs::path> file;
			//buf_pos pos;

			file_part() = delete;
			file_part(std::optional<fs::path> file, std::size_t line, std::size_t col);
			file_part(std::optional<fs::path> file, std::size_t line, std::size_t col, std::size_t length);
			file_part(fs::path file, std::size_t line, std::size_t col);
			file_part(fs::path file, std::size_t line, std::size_t col, std::size_t length);
			file_part(std::size_t line, std::size_t col);
			file_part(std::size_t line, std::size_t col, std::size_t length);
		};

		struct diagnostic {
			enum class level {
				error,
				warning,
				note,
			};

			level lvl;
			file_part part;
			std::string message;
			std::string primary_label;
			std::string secondary_label;
			std::vector<diagnostic> sub_diags;

			diagnostic(level lvl, file_part part, std::string message);
			diagnostic(level lvl, file_part part, std::string message, std::string primary_label);
			diagnostic(level lvl, file_part part, std::string message, std::string primary_label, std::string secondary_label);

			std::ostream& print(std::ostream& out, const std::vector<std::string>& lines) const;

			inline static diagnostic error(file_part part, std::string message,
										   std::string primary_label = std::string(),
										   std::string secondary_label = std::string()) {
				return diagnostic(level::error, std::move(part), std::move(message), std::move(primary_label),
								  std::move(secondary_label));
			}

			inline static diagnostic warn(file_part part, std::string message,
										   std::string primary_label = std::string(),
										   std::string secondary_label = std::string()) {
				return diagnostic(level::warning, std::move(part), std::move(message), std::move(primary_label),
								  std::move(secondary_label));
			}

			inline static diagnostic note(file_part part, std::string message,
										   std::string primary_label = std::string(),
										   std::string secondary_label = std::string()) {
				return diagnostic(level::note, std::move(part), std::move(message), std::move(primary_label),
								  std::move(secondary_label));
			}

		private:
			static term::control get_control(level lvl);
		};

		std::ostream& operator<<(std::ostream& stream, const diagnostic::level& level);

		class error : public std::runtime_error {
			diagnostic _diag;

		public:
			error(diagnostic diag);

			inline diagnostic& diag() { return _diag; }
			inline const diagnostic& diag() const { return _diag; }
		};

		template <class UnsignedType>
		UnsignedType parse_unsigned(const char* first, const char *last, const UnsignedType max_value, const char** strend) {
			UnsignedType value;

			const std::from_chars_result result = std::from_chars(first, last, value);

			if (result.ec == std::errc::invalid_argument)
				throw error(diagnostic::error(file_part(0, 1), "not a number",
											  "a number must only contain digits"));
			if (result.ec == std::errc::result_out_of_range || value > max_value)
				throw error(diagnostic::error(file_part(0, last - first), "number too large", std::format("maximum accepted value is {}", max_value)));
			if (result.ptr == first)
				throw error(diagnostic::error(file_part(0, 1), "not a number", "a number must contain at least one digit"));
			if (strend)
				*strend = result.ptr;

			return value;
		}

		template <class UnsignedType>
		UnsignedType parse_unsigned(std::string_view view, const UnsignedType max_value = std::numeric_limits<UnsignedType>::max()) {
			const char *end = nullptr;
			const auto result = parse_unsigned<UnsignedType>(view.data(), view.data() + view.length(), max_value, &end);
			if (end != view.data() + view.length())
				throw error(diagnostic::error(file_part(end - view.begin(), 1), "not a number",
											  "a number must only contain digits"));
			return result;
		}

		class parse_session {
			std::istream& _stream;
			std::vector<std::string> _lines;

			std::optional<fs::path> _file;
			std::size_t _col_num;

		public:
			parse_session() = delete;
			parse_session(std::istream& stream);

			std::optional<int> peek();
			std::optional<int> get();
			void expect(int ch);
			std::string get_word();
			std::string get_word_quoted();
			std::string get_word_simple();
			std::size_t ignore_ws();

			inline std::size_t column() const { return _col_num; }
			inline std::size_t line() const { return _lines.size() == 0 ? 0 : _lines.size() - 1; }
			inline const std::optional<fs::path>& file() const { return _file; }
			inline const std::vector<std::string> lines() const { return _lines; }

			inline diagnostic make_error(std::size_t line, std::size_t col, std::size_t length, std::string message,
										 std::string primary_label = std::string(), std::string secondary_label = std::string()) const {
				return diagnostic::error(file_part(file(), line, col, length), std::move(message),
										 std::move(primary_label), std::move(secondary_label));
			}
			inline diagnostic make_warn(std::size_t line, std::size_t col, std::size_t length, std::string message,
										 std::string primary_label = std::string(), std::string secondary_label = std::string()) const {
				return diagnostic::warn(file_part(file(), line, col, length), std::move(message),
										 std::move(primary_label), std::move(secondary_label));
			}

		private:
			std::string_view get_line(std::size_t idx) const;
			std::string_view cur_line() const { return get_line(line()); }

			std::optional<std::string> read_line();

			inline std::string_view remaining() const {
				if (line() < _lines.size())
					return std::string_view(cur_line().begin() + column(), cur_line().end());
				return std::string_view();
			}
			std::string_view fill_buf();
			void consume(std::size_t count);

			inline diagnostic make_error(std::size_t line, std::size_t col, std::string message,
										 std::string primary_label = std::string()) const {
				return make_error(line, col, 1, std::move(message), std::move(primary_label));
			};
			inline diagnostic make_error(std::string message, std::string primary_label = std::string()) const {
				return make_error(line(), column(), std::move(message), std::move(primary_label));
			};
		};

		class listen_address {
			std::string _node;
			port _service;

			constexpr listen_address() noexcept = default;
		public:
			constexpr listen_address(std::string node, port service) noexcept;
			constexpr listen_address(port service) noexcept;

			inline std::string_view node() const { return _node; }
			inline port service() const { return _service; }

			inline constexpr auto operator<=>(const listen_address& other) const {
				if (node() == other.node())
					return service() <=> other.service();
				return node() <=> other.node();
			}

			static listen_address parse(parse_session& session);
		};

		struct filter {
			enum class type {
				location,
				method,
			};
			type type;
			std::string match;

			std::strong_ordering operator<=>(const filter& other) const;
		};

		template <class T>
		struct define {
			T def;
			file_part part;

			constexpr operator T() const noexcept { return def; };
		};

		template <class T>
		define<T> make_define(T def, file_part part) {
			return define<T>{std::move(def), std::move(part)};
		}

		struct config_path {
			fs::path path;

			inline static config_path parse(parse_session& session) {
				return config_path{session.get_word()};
			}
		};

		class block_config {
			std::optional<std::pair<std::size_t, file_part>> _max_body_size;
			std::unordered_map<std::string, std::string> _headers;
			std::optional<define<config_path>> _index;
			std::optional<define<config_path>> _root;
			std::optional<std::pair<unsigned short, std::string>> _redirect;//TODO use http_response_code
			std::map<filter, define<block_config>> _filters;

		public:
			static block_config parse(parse_session& session);

			inline std::optional<std::size_t> max_body_size() const { if (_max_body_size) { return _max_body_size->first; } else { return std::nullopt; } }
			inline const auto& filters() const { return _filters; }

		protected:
			void parse_max_body_size(parse_session& session);
			void parse_location(parse_session& session);
			void parse_root(parse_session& session);
			void parse_index(parse_session& session);

			template <class T>
			define<T> parse_define(parse_session& session, const std::string& directive) {
				file_part part(session.file(), session.line(), session.column() - directive.length(), directive.length());
				session.ignore_ws();
				return {T::parse(session), std::move(part)};
			}
		};

		class server_config : public block_config {
			std::optional<define<std::string>> _server_name;
			std::vector<listen_address> _listen;

			server_config() = default;

		public:
			static server_config parse(parse_session& session);

			std::string_view server_name() const;
			inline const std::vector<listen_address>& addresses() const { return _listen; }

		private:
			void parse_listen(parse_session& session);
			void parse_ssl(parse_session& session);
			void parse_server_name(parse_session& session);
		};
	};
} // namespace cobra

template<>
struct std::formatter<cobra::config::diagnostic::level, char> {

	constexpr auto parse(std::format_parse_context& fpc) {
		return fpc.begin();
	}

	template<class FormatContext>
	constexpr auto format(cobra::config::diagnostic::level lvl, FormatContext& fc) const {
		using namespace cobra::config;
		switch (lvl) {
		case diagnostic::level::error:
			return std::format_to(fc.out(), "error");
		case diagnostic::level::warning:
			return std::format_to(fc.out(), "warning");
		case diagnostic::level::note:
			return std::format_to(fc.out(), "note");
		}
	}
};

#endif
