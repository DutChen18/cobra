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
#include <deque>
#include <exception>
#include <filesystem>
#include <format>
#include <functional>
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

extern "C" {
#include <sys/stat.h>
}

#define COBRA_BLOCK_KEYWORDS                                                                                           \
	X(max_body_size)                                                                                                   \
	X(location)                                                                                                        \
	X(index)                                                                                                           \
	X(cgi)                                                                                                             \
	X(fast_cgi)                                                                                                        \
	X(root)                                                                                                            \
	X(static)                                                                                                          \
	X(extension)

#define COBRA_SERVER_KEYWORDS                                                                                          \
	X(listen)                                                                                                          \
	X(ssl)                                                                                                             \
	X(server_name)                                                                                                     \
	COBRA_BLOCK_KEYWORDS

namespace cobra {
	namespace fs = std::filesystem;
	using port = unsigned short;

	namespace config {

		std::size_t levenshtein_dist(const std::string& a, const std::string& b);
		template <typename Container>
		const std::string get_suggestion(const Container& c, const std::string &str) {
			return *std::min_element(c.begin(), c.end(), [str](auto a, auto b) { return levenshtein_dist(a, str) < levenshtein_dist(b, str); });
		}

		//TODO multiline diagnostics
		struct buf_pos {
			std::size_t line;
			std::size_t col;

			buf_pos() = delete;
			constexpr buf_pos(std::size_t line, std::size_t col) noexcept;

			auto operator<=>(const buf_pos& other) const = default;
		};

		struct file_part {
			std::optional<fs::path> file;
			buf_pos start, end;

			file_part() = delete;
			file_part(std::optional<fs::path> file, buf_pos start, buf_pos end);
			file_part(std::optional<fs::path> file, std::size_t line, std::size_t col);
			file_part(std::optional<fs::path> file, std::size_t line, std::size_t col, std::size_t length);
			file_part(std::size_t line, std::size_t col);
			file_part(std::size_t line, std::size_t col, std::size_t length);

			inline bool is_multiline() const { return start.line != end.line; }

			auto operator<=>(const file_part& other) const = default;
		};

		//replaces tabs with single space in formatter
		struct config_line {
			std::string_view data;
		};

		class line_printer {
		protected:
			std::ostream& _stream;
			std::size_t _max_value;
			std::size_t _max_width;

		public:
			line_printer(const line_printer& other);
			line_printer(std::ostream& stream, std::size_t max_line);
			virtual ~line_printer();

			virtual line_printer& print();
			virtual line_printer& print(std::size_t line_num);

			template <class... Args>
			line_printer& print(std::format_string<Args...> fmt, Args&&... args) {
				this->print();
				cobra::print(_stream, fmt, std::forward<Args>(args)...);
				return *this;
			}

			template <class... Args>
			line_printer& print(std::size_t line_num, std::format_string<Args...> fmt, Args&&... args) {
				this->print(line_num);
				cobra::print(_stream, fmt, std::forward<Args>(args)...);
				return *this;
			}

			line_printer& println();
			line_printer& println(std::size_t line_num);

			template <class... Args>
			line_printer& println(std::format_string<Args...> fmt, Args&&... args) {
				this->print(fmt, std::forward<Args>(args)...);
				return newline();
			}

			template <class... Args>
			line_printer& println(std::size_t line_num, std::format_string<Args...> fmt, Args&&... args) {
				this->print(line_num, fmt, std::forward<Args>(args)...);
				return newline();
			}

			inline std::size_t max_value() const { return _max_value; }
			inline std::ostream& stream() const { return _stream; }

		private:
			line_printer& newline();
		};

		class multiline_printer : public line_printer {
			std::size_t _leading_spaces;
			term::control _style;

			multiline_printer(term::control style, std::span<const std::string> lines);
		public:
			multiline_printer(const line_printer& printer, term::control style, std::span<const std::string> lines);
			multiline_printer(std::ostream& stream, std::size_t max_line, term::control style, std::span<const std::string> lines);

			multiline_printer& print() override;
			multiline_printer& print(std::size_t line_num) override;

			inline config_line trim_line(std::string_view view) const {
				return config_line{view.substr(_leading_spaces)};
			}

			inline line_printer& inner() { return *this; }
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
		private:
			std::ostream& print_single(std::ostream& out, const std::vector<std::string>& lines) const;
			void print_single_diag(line_printer& printer, std::size_t offset = 0) const;
			std::ostream& print_multi(std::ostream& out, const std::vector<std::string>& lines,
									  std::deque<std::reference_wrapper<const diagnostic>> inline_diags =
										  std::deque<std::reference_wrapper<const diagnostic>>()) const;

		public:

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
			static term::control get_color(level lvl);
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

		class parse_session;

		class diagnostic_reporter {
		public:
			inline virtual ~diagnostic_reporter() {}
			virtual void report(const diagnostic& diag, const parse_session& session) = 0;
			virtual void report_token(file_part part, std::string type, const parse_session& session) = 0;
			virtual void report_inlay_hint(buf_pos pos, std::string hint, const parse_session& session) = 0;
		};

		class basic_diagnostic_reporter : public diagnostic_reporter {
			bool _print;
			std::vector<diagnostic> _diags;
			std::vector<std::pair<file_part, std::string>> _tokens;
			std::vector<std::pair<buf_pos, std::string>> _inlay_hints;

		public:
			basic_diagnostic_reporter(bool print);

			void report(const diagnostic& diag, const parse_session& session) override;
			void report_token(file_part part, std::string type, const parse_session& session) override;
			void report_inlay_hint(buf_pos pos, std::string hint, const parse_session& session) override;

			inline const std::vector<diagnostic>& get_diags() const {
				return _diags;
			}

			inline const std::vector<std::pair<file_part, std::string>> get_tokens() const {
				return _tokens;
			}

			inline const std::vector<std::pair<buf_pos, std::string>> get_inlay_hints() const {
				return _inlay_hints;
			}
		};

		class word {
			file_part _part;
			std::string _str;

			word(file_part part, std::string str);
		public:
			friend class parse_session;

			inline const file_part& part() const { return _part; }
			inline const std::string& str() const { return _str; }
			inline constexpr operator std::string() const & { return _str; }
			inline constexpr operator std::string() const && { return std::move(_str); }

			bool operator==(const word& other) const = default;
		};

		class parse_session {
			std::istream& _stream;
			std::vector<std::string> _lines;
			diagnostic_reporter* _reporter;

			std::optional<fs::path> _file;
			std::size_t _col_num;

		public:
			parse_session() = delete;
			parse_session(std::istream& stream, diagnostic_reporter& reporter);

			std::optional<int> peek();
			std::optional<int> get();
			void expect(int ch);
			void expect_word_simple(std::string_view str);
			word get_word();
			word get_word_simple();
			void expect_word_simple(std::string_view str, std::string type);
			word get_word(std::string type);
			word get_word_simple(std::string type);
			void expect_word_simple(std::string_view str, std::string type, std::string hint);
			word get_word(std::string type, std::string hint);
			word get_word_simple(std::string type, std::string hint);
			std::size_t ignore_ws();
			std::size_t ignore_line();

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

			inline void report(const diagnostic& diag) const {
				_reporter->report(diag, *this);
			}
			inline void report_token(file_part part, std::string type) const {
				_reporter->report_token(part, std::move(type), *this);
			}
			inline void report_inlay_hint(buf_pos pos, std::string hint) const {
				_reporter->report_inlay_hint(pos, std::move(hint), *this);
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

		private:
			word get_word_quoted();
			std::string_view get_line(std::size_t idx) const;
			inline std::string_view cur_line() const {
				return get_line(line());
			}

			std::optional<std::string> read_line();

			inline bool is_simple_word_char(int ch) {
				return std::isgraph(ch) && ch != '/' && ch != '*';
			}

			inline std::string_view remaining() const {
				if (line() < _lines.size())
					return std::string_view(cur_line().begin() + column(), cur_line().end());
				return std::string_view();
			}
			std::string_view fill_buf();
			std::size_t consume(std::size_t count);

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

			inline const std::string& node() const {
				return _node;
			}
			inline port service() const {
				return _service;
			}

			auto operator<=>(const listen_address& other) const = default;

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

			static define parse(parse_session& session, const std::string& directive) {
				file_part part(session.file(), session.line(), session.column() - directive.length(),
							   directive.length());
				session.ignore_ws();
				return define<T>(T::parse(session), std::move(part));
			}

			constexpr operator T() const noexcept {
				return def;
			};

			constexpr T* operator->() noexcept {
				return &def;
			}

			constexpr const T* operator->() const noexcept {
				return &def;
			}

			inline bool operator==(const define& other) const requires std::equality_comparable<T> {
				return def == other.def;
			}

			inline constexpr auto operator<=>(const define& other) const noexcept {
				return def <=> other.def;
			}
		};

		template <class T>
		define<T> make_define(T def, file_part part) {
			return define<T>{std::move(def), std::move(part)};
		}

		class config_file {
			fs::path _file;

		protected:
			config_file(fs::path file);

		public:
			static config_file parse(parse_session& session);

			inline const fs::path& file() const { return _file; }

			inline operator fs::path() noexcept {
				return _file;
			}

			auto operator<=>(const config_file& other) const = default;
		};

		class config_exec : public config_file {
			config_exec(fs::path file);
		public:
			static config_exec parse(parse_session& session);
		};

		class config_dir {
			fs::path _dir;

			config_dir(fs::path dir);
		public:
			static config_dir parse(parse_session& session);

			inline const fs::path& dir() const { return _dir; }

			inline operator fs::path() noexcept {
				return _dir;
			}

			auto operator<=>(const config_dir& other) const = default;
		};

		struct static_file_config {
			inline static static_file_config parse(parse_session& session) {
				(void) session;
				return {};
			}

			auto operator<=>(const static_file_config& other) const = default;
		};

		struct extension {
			std::string ext;

			inline static extension parse(parse_session& session) {
				return { session.get_word_simple("string", "extension") };
			}

			auto operator<=>(const extension& other) const = default;
		};

		struct cgi_config {
			config_exec command;

			inline static cgi_config parse(parse_session& session) {
				return {config_exec::parse(session)};
			}

			auto operator<=>(const cgi_config& other) const = default;
		};

		struct fast_cgi_config {
			listen_address address;

			inline static fast_cgi_config parse(parse_session& session) {
				return {listen_address::parse(session)};
			}

			auto operator<=>(const fast_cgi_config& other) const = default;
		};

		struct server_name {
			std::string name;

			inline static server_name parse(parse_session& session) {
				return {session.get_word_simple("string", "host")};
			}

			auto operator<=>(const server_name& other) const = default;
		};

		struct location_filter {
			uri_abs_path path;

			constexpr auto operator<=>(const location_filter& other) const noexcept = default;

			inline static location_filter parse(parse_session& session) {
				return {fs::path(session.get_word("filter", "filter"))};
			}

			bool starts_with(const location_filter& other) const;
		};

		struct method_filter {
			std::string method;

			constexpr auto operator<=>(const method_filter& other) const noexcept = default;

			inline static method_filter parse(parse_session& session) {
				return {session.get_word_simple("filter", "filter")};
			}
		};

		class ssl_config {
			config_file _cert;
			config_file _key;

			ssl_config(config_file cert, config_file key);

		public:
			static ssl_config parse(parse_session& session);

			inline const fs::path& cert() const { return _cert.file(); }
			inline const fs::path& key() const { return _key.file(); }

			auto operator<=>(const ssl_config& other) const = default;
		};

		class server;

		class block_config {
			using filter_type = std::variant<location_filter, method_filter>;

		protected:
			std::optional<filter_type> _filter;
			std::optional<define<std::size_t>> _max_body_size;
			std::unordered_map<http_header_key, define<http_header_value>> _headers;
			std::optional<define<config_file>> _index;
			std::optional<define<config_dir>> _root;
			std::optional<define<std::variant<static_file_config, cgi_config, fast_cgi_config>>>
				_handler; // TODO add other handlers (redirect, proxy...)
			std::vector<std::pair<filter_type, define<block_config>>> _filters;
			std::unordered_map<std::string, file_part> _server_names;
			std::set<define<extension>> _extensions;

		public:
			static define<block_config> parse(parse_session& session);

			bool operator==(const block_config& other) const = default;

			friend class config;
			friend class server_config;

		protected:
			void parse_max_body_size(parse_session& session);
			void parse_location(parse_session& session);
			void parse_static(parse_session& session);
			void parse_extension(parse_session& session);
			void parse_cgi(parse_session& session);
			void parse_fast_cgi(parse_session& session);
			void parse_index(parse_session& session);
			void parse_root(parse_session& session);
			void parse_server_name(parse_session& session);
			void parse_comment(parse_session& session);

			template <class Container>
			static void throw_undefined_directive(const Container& c, const word &w) {
				throw error(diagnostic::error(w.part(), std::format("unknown directive `{}`", w.str()),
											  std::format("did you mean `{}`?", get_suggestion(c, w.str()))));
			}

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
			static define<server_config> parse(parse_session& session);
			static std::vector<server_config> parse_servers(parse_session& session);

			bool operator==(const server_config& other) const = default;

			static void lint_configs(const std::vector<define<server_config>>& configs, const parse_session& session);
		private:
			static void empty_filters_lint(const std::vector<define<server_config>>& configs, const parse_session& session);
			static void empty_filters_lint(const define<block_config>& config, const parse_session& session);
			static void ssl_lint(const std::vector<define<server_config>>& configs);
			static void server_name_lint(const std::vector<define<server_config>>& configs);
			static void not_listening_server_lint(const std::vector<define<server_config>>& configs,
												  const parse_session& session);
			static void unrooted_handler_lint(const std::vector<define<server_config>>& configs);

			template <class Config>
			static void unrooted_handler_lint(const define<Config>& config)
			/*requires std::derived_from<Config, block_config>*/
			{
				if (config->_root)
					return;

				if (config->_handler && !config->_root) {
					// TODO this should not trigger for reverse proxy
					diagnostic diag = diagnostic::error(config->_handler->part, "unrooted handler",
														"consider rooting it using: `root`");
					throw error(diag);
				}

				for (auto& [_, sub_cfg] : config->_filters) {
					unrooted_handler_lint(sub_cfg);
				}
			}

			//static void unrooted_handler_lint(const define<server_config>& config);
			//static void unrooted_handler_lint(const define<block_config>& config);
			//TODO add lint for non rooted handlers (static, cgi, fast_cgi)
			void parse_listen(parse_session& session);
			void parse_ssl(parse_session& session);
		};

		class config {
		public:
			config* parent;

			std::optional<std::size_t> max_body_size;
			std::optional<fs::path> index;
			std::optional<fs::path> root;
			std::optional<std::variant<static_file_config, cgi_config, fast_cgi_config>> handler;//TODO use configs from handler.hh
			std::unordered_set<http_request_method> methods;
			std::unordered_set<std::string> extensions;
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

			virtual void debug_print(std::ostream& stream, std::size_t depth = 0) const;
		};

		class server : public config {
		public:
			std::vector<listen_address> addresses;
			std::optional<ssl_config> ssl;

			server(const server_config& cfg);

			void debug_print(std::ostream& stream, std::size_t depth = 0) const override;
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

template <>
struct std::formatter<cobra::config::listen_address, char> {
	constexpr auto parse(std::format_parse_context& fpc) {
		return fpc.begin();
	}

	template <class FormatContext>
	constexpr auto format(cobra::config::listen_address address, FormatContext& fc) const {
		return std::format_to(fc.out(), "{}:{}", address.node(), address.service());
	}
};

template <>
struct std::formatter<cobra::config::config_line, char> {
	constexpr auto parse(std::format_parse_context& fpc) {
		return fpc.begin();
	}

	template <class FormatContext>
	constexpr auto format(cobra::config::config_line line, FormatContext& fc) const {
		for (auto& ch : line.data) {
			if (ch == '\t') {
				std::format_to(fc.out(), " ");
			} else {
				std::format_to(fc.out(), "{}", ch);
			}
		}
		return fc.out();
	}
};

#endif
