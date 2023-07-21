#ifndef COBRA_CONFIG_HH
#define COBRA_CONFIG_HH

#include "cobra/http/message.hh"
#include "cobra/http/uri.hh"
#include "cobra/print.hh"

//TODO check which headers aren't needed anymore
#include <algorithm>
#include <any>
#include <cctype>
#include <charconv>
#include <compare>
#include <concepts>
#include <exception>
#include <filesystem>
#include <format>
#include <iostream>
#include <istream>
#include <limits>
#include <map>
#include <optional>
#include <ostream>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#define COBRA_BLOCK_KEYWORDS                                                                                           \
	X(max_body_size)                                                                                                   \
	X(location)                                                                                                        \
	X(index)                                                                                                           \
	X(server_name)                                                                                                     \
	X(root)

#define COBRA_SERVER_KEYWORDS                                                                                          \
	X(listen)                                                                                                          \
	X(ssl)                                                                                                             \
	COBRA_BLOCK_KEYWORDS

namespace cobra {
	namespace fs = std::filesystem;
	using port = unsigned short;

	namespace config {

		//TODO multiline diagnostics
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
			diagnostic(level lvl, file_part part, std::string message, std::string primary_label,
					   std::string secondary_label);

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

			inline diagnostic& diag() {
				return _diag;
			}
			inline const diagnostic& diag() const {
				return _diag;
			}
		};

		template <class UnsignedType>
		UnsignedType parse_unsigned(const char* first, const char* last, const UnsignedType max_value,
									const char** strend) {
			UnsignedType value;

			const std::from_chars_result result = std::from_chars(first, last, value);

			if (result.ec == std::errc::invalid_argument)
				throw error(diagnostic::error(file_part(0, 1), "not a number", "a number must only contain digits"));
			if (result.ec == std::errc::result_out_of_range || value > max_value)
				throw error(diagnostic::error(file_part(0, last - first), "number too large",
											  std::format("maximum accepted value is {}", max_value)));
			if (result.ptr == first)
				throw error(
					diagnostic::error(file_part(0, 1), "not a number", "a number must contain at least one digit"));
			if (strend)
				*strend = result.ptr;

			return value;
		}

		template <class UnsignedType>
		UnsignedType parse_unsigned(std::string_view view,
									const UnsignedType max_value = std::numeric_limits<UnsignedType>::max()) {
			const char* end = nullptr;
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
			void expect_word_simple(std::string_view str);
			std::string get_word();
			std::string get_word_simple();
			std::size_t ignore_ws();

			inline std::size_t column() const {
				return _col_num;
			}
			inline std::size_t line() const {
				return _lines.size() == 0 ? 0 : _lines.size() - 1;
			}
			inline const std::optional<fs::path>& file() const {
				return _file;
			}
			inline const std::vector<std::string> lines() const {
				return _lines;
			}

			inline bool eof() const { return _stream.eof(); }

			inline diagnostic make_error(std::size_t line, std::size_t col, std::size_t length, std::string message,
										 std::string primary_label = std::string(),
										 std::string secondary_label = std::string()) const {
				return diagnostic::error(file_part(file(), line, col, length), std::move(message),
										 std::move(primary_label), std::move(secondary_label));
			}
			inline diagnostic make_warn(std::size_t line, std::size_t col, std::size_t length, std::string message,
										std::string primary_label = std::string(),
										std::string secondary_label = std::string()) const {
				return diagnostic::warn(file_part(file(), line, col, length), std::move(message),
										std::move(primary_label), std::move(secondary_label));
			}

			inline std::ostream& print_diagnostic(std::ostream& stream, const diagnostic& diag) const {
				return diag.print(stream, lines());
			}

		private:
			std::string get_word_quoted();
			std::string_view get_line(std::size_t idx) const;
			inline std::string_view cur_line() const {
				return get_line(line());
			}

			std::optional<std::string> read_line();

			inline std::string_view remaining() const {
				if (line() < _lines.size())
					return std::string_view(cur_line().begin() + column(), cur_line().end());
				return std::string_view();
			}
			std::string_view fill_buf();
			void consume(std::size_t count);

			inline diagnostic make_error(std::size_t line, std::size_t col, std::string message,
										 std::string primary_label = std::string(),
										 std::string secondary_label = std::string()) const {
				return make_error(line, col, 1, std::move(message), std::move(primary_label),
								  std::move(secondary_label));
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

			inline std::string_view node() const {
				return _node;
			}
			inline port service() const {
				return _service;
			}

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

			constexpr define(const T& t, const file_part& part) : def(t), part(part) {}

			constexpr define(T&& t, file_part&& part) noexcept : def(std::move(t)), part(std::move(part)) {}

			template <class U>
			constexpr define(const define<U>& other) noexcept(def(other.def))
				requires std::convertible_to<U, T>
				: def(other.def), part(other.part) {}

			template <class U>
			constexpr define(define<U>&& other) noexcept
				requires std::convertible_to<U, T>
				: def(std::move(other.def)), part(std::move(other.part)) {}

			constexpr operator T() const noexcept {
				return def;
			};

			constexpr T* operator->() noexcept {
				return &def;
			}

			constexpr const T* operator->() const noexcept {
				return &def;
			}

			inline constexpr auto operator<=>(const define& other) const noexcept {
				return def <=> other.def;
			}
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

			operator fs::path() noexcept {
				return path;
			}
		};

		struct static_file_config {
			config_path root;

			inline static static_file_config parse(parse_session& session) {
				return {config_path{session.get_word()}};
			}
		};

		struct cgi_config {};

		struct server_name {
			std::string name;

			inline static server_name parse(parse_session& session) {
				return {session.get_word_simple()};
			}
		};

		struct location_filter {
			uri_abs_path path;

			constexpr auto operator<=>(const location_filter& other) const noexcept = default;

			inline static location_filter parse(parse_session& session) {
				return {fs::path(session.get_word())};
			}

			bool starts_with(const location_filter& other) const;
		};

		struct method_filter {
			std::string method;

			constexpr auto operator<=>(const method_filter& other) const noexcept = default;

			inline static method_filter parse(parse_session& session) {
				return {session.get_word_simple()};
			}
		};

		struct ssl_config {
			fs::path cert;
			fs::path key;

			static ssl_config parse(parse_session& session);
		};

		class server;

		class block_config {
			using filter_type = std::variant<location_filter, method_filter>;

		protected:
			std::optional<filter_type> _filter;
			std::optional<define<std::size_t>> _max_body_size;
			std::unordered_map<http_header_key, define<http_header_value>> _headers;
			std::optional<define<config_path>> _index;
			std::optional<define<std::variant<static_file_config, cgi_config>>>
				_handler; // TODO add other handlers (redirect, proxy...)
			//std::map<filter_type, define<block_config>> _filters;
			std::vector<std::pair<filter_type, define<block_config>>> _filters;
			std::unordered_map<std::string, file_part> _server_names;

		public:
			static block_config parse(parse_session& session);

			friend class config;

		protected:
			void parse_max_body_size(parse_session& session);
			void parse_location(parse_session& session);
			void parse_root(parse_session& session);
			void parse_index(parse_session& session);
			void parse_server_name(parse_session& session);

			template <class T>
			define<T> parse_define(parse_session& session, const std::string& directive) {
				file_part part(session.file(), session.line(), session.column() - directive.length(),
							   directive.length());
				session.ignore_ws();
				return define<T>(T::parse(session), std::move(part));
			}
		};

		class server_config : protected block_config {
			std::set<define<listen_address>> _addresses;
			std::optional<define<ssl_config>> _ssl;

			server_config() = default;

			friend class server;

		public:
			static server_config parse(parse_session& session);
			static std::vector<server_config> parse_servers(parse_session& session);

			static void lint_configs(const std::vector<server_config>& configs, const parse_session& session);
		private:
			static void ssl_lint(const std::vector<server_config>& configs);
			static void server_name_lint(const std::vector<server_config>& configs);
			void parse_listen(parse_session& session);
			void parse_ssl(parse_session& session);
		};

		class config {
		public:
			config* parent;

			std::optional<std::size_t> max_body_size;
			std::optional<fs::path> index;
			std::optional<std::variant<static_file_config, cgi_config>> handler;
			std::unordered_set<http_request_method> methods;
			std::unordered_set<std::string> server_names;
			std::vector<std::shared_ptr<config>> sub_configs;

			http_header_map headers;
			uri_abs_path location;

		private:
			config(config* parent, const block_config& cfg);

		public:
			config() = default;
			config(const block_config& cfg);
			virtual ~config();
		};

		class server : public config {
		public:
			std::vector<listen_address> addresses;

			server(const server_config& cfg);
		};
	}; // namespace config
} // namespace cobra

template <>
struct std::formatter<cobra::config::diagnostic::level, char> {
	constexpr auto parse(std::format_parse_context& fpc) {
		return fpc.begin();
	}

	template <class FormatContext>
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
