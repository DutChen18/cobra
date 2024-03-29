#ifndef COBRA_CONFIG_HH
#define COBRA_CONFIG_HH

#include "cobra/http/handler.hh"
#include "cobra/http/message.hh"
#include "cobra/http/uri.hh"
#include "cobra/print.hh"
#include "cobra/text.hh"

#include <algorithm>
#include <compare>
#include <deque>
#include <exception>
#include <map>
#include <memory>
#include <set>
#include <string>
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
	X(error_page)                                                                                                      \
	X(location)                                                                                                        \
	X(method)                                                                                                          \
	X(index)                                                                                                           \
	X(cgi)                                                                                                             \
	X(fast_cgi)                                                                                                        \
	X(root)                                                                                                            \
	X(redirect)                                                                                                        \
	X(set_header)                                                                                                      \
	X(static)                                                                                                          \
	X(proxy)                                                                                                           \
	X(extension)

#ifndef COBRA_NO_SSL
#define COBRA_SERVER_KEYWORDS                                                                                          \
	X(listen)                                                                                                          \
	X(ssl)                                                                                                             \
	X(server_name)                                                                                                     \
	COBRA_BLOCK_KEYWORDS
#else
#define COBRA_SERVER_KEYWORDS                                                                                          \
	X(listen)                                                                                                          \
	X(server_name)                                                                                                     \
	COBRA_BLOCK_KEYWORDS
#endif

namespace cobra {
	namespace config {
		template <class T>
		struct define;
	}
} // namespace cobra

template <class T>
struct std::hash<cobra::config::define<T>> : public std::hash<T> {
	hash() : std::hash<T>() {}
	hash(const hash& other) : std::hash<T>(other) {}
	~hash() {}

	std::size_t operator()(const hash& def) const {
		return std::hash<T>::operator()(def.def);
	}
};

namespace cobra {
	namespace fs = std::filesystem;
	using port = unsigned short;

	namespace config {

		std::size_t levenshtein_dist(const std::string& a, const std::string& b);
		template <typename Container>
		const std::string get_suggestion(const Container& c, const std::string& str) {
			return *std::min_element(c.begin(), c.end(), [str](auto a, auto b) {
				return levenshtein_dist(a, str) < levenshtein_dist(b, str);
			});
		}

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

			inline bool is_multiline() const {
				return start.line != end.line;
			}

			auto operator<=>(const file_part& other) const {
				if (file != other.file)
					return file < other.file ? std::strong_ordering::less : std::strong_ordering::greater;
				if (start != other.start)
					return start < other.start ? std::strong_ordering::less : std::strong_ordering::greater;
				if (end != other.end)
					return end < other.end ? std::strong_ordering::less : std::strong_ordering::greater;
				return std::strong_ordering::equal;
			}

			bool operator==(const file_part& other) const = default;
			bool operator!=(const file_part& other) const = default;
			bool operator>(const file_part& other) const = default;
			bool operator<(const file_part& other) const = default;
			bool operator>=(const file_part& other) const = default;
			bool operator<=(const file_part& other) const = default;
		};

		// replaces tabs with single space in formatter
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
				cobra::print(_stream, std::move(fmt), std::forward<Args>(args)...);
				return *this;
			}

			template <class... Args>
			line_printer& print(std::size_t line_num, std::format_string<Args...> fmt, Args&&... args) {
				this->print(line_num);
				cobra::print(_stream, std::move(fmt), std::forward<Args>(args)...);
				return *this;
			}

			line_printer& println();
			line_printer& println(std::size_t line_num);

			template <class... Args>
			line_printer& println(std::format_string<Args...> fmt, Args&&... args) {
				this->print(std::move(fmt), std::forward<Args>(args)...);
				return newline();
			}

			template <class... Args>
			line_printer& println(std::size_t line_num, std::format_string<Args...> fmt, Args&&... args) {
				this->print(line_num, std::move(fmt), std::forward<Args>(args)...);
				return newline();
			}

			inline std::size_t max_value() const {
				return _max_value;
			}
			inline std::ostream& stream() const {
				return _stream;
			}

		private:
			line_printer& newline();
		};

		class multiline_printer : public line_printer {
			std::size_t _leading_spaces;
			term::control _style;

			multiline_printer(term::control style, std::span<const std::string> lines);

		public:
			multiline_printer(const line_printer& printer, term::control style, std::span<const std::string> lines);
			multiline_printer(std::ostream& stream, std::size_t max_line, term::control style,
							  std::span<const std::string> lines);

			multiline_printer& print() override;
			multiline_printer& print(std::size_t line_num) override;

			inline config_line trim_line(std::string_view view) const {
				return config_line{view.substr(_leading_spaces)};
			}

			inline line_printer& inner() {
				return *this;
			}
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

		inline const char* diagnostic_level_name(const diagnostic::level& level) {
			using namespace cobra::config;
			switch (level) {
			case diagnostic::level::error:
				return "error";
			case diagnostic::level::warning:
				return "warning";
			case diagnostic::level::note:
				return "note";
			}
			std::terminate();
		}

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
				throw error(diagnostic::error(file_part(0, 1), COBRA_TEXT("not a number"),
											  COBRA_TEXT("a number must only contain digits")));
			if (result.ec == std::errc::result_out_of_range || value > max_value)
				throw error(diagnostic::error(file_part(0, last - first), COBRA_TEXT("number too large"),
											  COBRA_TEXT("maximum accepted value is {}", max_value)));
			if (result.ptr == first)
				throw error(diagnostic::error(file_part(0, 1), COBRA_TEXT("not a number"),
											  COBRA_TEXT("a number must contain at least one digit")));
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
				throw error(diagnostic::error(file_part(end - view.begin(), 1), COBRA_TEXT("not a number"),
											  COBRA_TEXT("a number must only contain digits")));
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

			inline const file_part& part() const {
				return _part;
			}
			inline const std::string& str() const {
				return _str;
			}
			inline constexpr operator std::string() const& {
				return _str;
			}
			inline constexpr operator std::string() const&& noexcept {
				return std::move(_str);
			}

			inline constexpr std::string inner() && noexcept {
				return std::move(_str);
			}

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

			inline bool eof() const {
				return _stream.eof();
			}

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

			auto operator<=>(const listen_address& other) const {
				if (*this < other) {
					return std::strong_ordering::less;
				} else if (*this > other) {
					return std::strong_ordering::greater;
				} else {
					return std::strong_ordering::equal;
				}
			}

			bool operator==(const listen_address& other) const = default;
			bool operator!=(const listen_address& other) const = default;

			bool operator<(const listen_address& other) const {
				return _node < other._node || (!(other._node < _node) && _service < other._service);
			}

			bool operator>(const listen_address& other) const {
				return other < *this;
			}

			bool operator>=(const listen_address& other) const {
				return !(*this < other);
			}

			bool operator<=(const listen_address& other) const {
				return !(other < *this);
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
			constexpr define(const define<U>& other) noexcept
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

			constexpr T inner() && noexcept {
				return std::move(def);
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

			inline bool operator==(const define& other) const
				requires std::equality_comparable<T>
			{
				return def == other.def;
			}

			inline bool operator==(const T& t) const
				requires std::equality_comparable<T>
			{
				return def == t;
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

			inline const fs::path& file() const {
				return _file;
			}

			inline operator fs::path() noexcept {
				return _file;
			}

			auto operator<=>(const config_file& other) const {
				if (_file != other._file)
					return _file < other._file ? std::strong_ordering::less : std::strong_ordering::greater;
				return std::strong_ordering::equal;
			}

			bool operator==(const config_file& other) const = default;
			bool operator!=(const config_file& other) const = default;
			bool operator>(const config_file& other) const = default;
			bool operator<(const config_file& other) const = default;
			bool operator>=(const config_file& other) const = default;
			bool operator<=(const config_file& other) const = default;
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

			inline const fs::path& dir() const {
				return _dir;
			}

			inline operator fs::path() noexcept {
				return _dir;
			}

			auto operator<=>(const config_dir& other) const {
				if (_dir != other._dir)
					return _dir < other._dir ? std::strong_ordering::less : std::strong_ordering::greater;
				return std::strong_ordering::equal;
			}

			bool operator==(const config_dir& other) const = default;
			bool operator!=(const config_dir& other) const = default;
			bool operator>(const config_dir& other) const = default;
			bool operator<(const config_dir& other) const = default;
			bool operator>=(const config_dir& other) const = default;
			bool operator<=(const config_dir& other) const = default;
		};

		struct static_file_config {
			bool list_dir;

			inline static static_file_config parse(parse_session& session) {
				auto w = session.get_word_simple("string", "list directory");
				if (w.str() == "true") {
					return {true};
				} else if (w.str() == "false") {
					return {false};
				} else {
					diagnostic diag = diagnostic::error(w.part(), COBRA_TEXT("invalid directory listing option"),
														COBRA_TEXT("expected either `true` or `false`"));
					throw error(diag);
				}
			}

			auto operator<=>(const static_file_config& other) const = default;
		};

		struct proxy_config {
			listen_address address;

			inline static proxy_config parse(parse_session& session) {
				return {listen_address::parse(session)};
			}

			auto operator<=>(const proxy_config& other) const = default;
		};

		struct redirect_config {
			http_response_code code;
			std::string location;

			static redirect_config parse(parse_session& session);

			auto operator<=>(const redirect_config& other) const {
				if (code != other.code)
					return code < other.code ? std::strong_ordering::less : std::strong_ordering::greater;
				if (location != other.location)
					return location < other.location ? std::strong_ordering::less : std::strong_ordering::greater;
				return std::strong_ordering::equal;
			}

			bool operator==(const redirect_config& other) const = default;
			bool operator!=(const redirect_config& other) const = default;
			bool operator>(const redirect_config& other) const = default;
			bool operator<(const redirect_config& other) const = default;
			bool operator>=(const redirect_config& other) const = default;
			bool operator<=(const redirect_config& other) const = default;
		};

		struct extension {
			std::string ext;

			inline static extension parse(parse_session& session) {
				return {session.get_word_simple("string", "extension")};
			}

			auto operator<=>(const extension& other) const {
				if (ext != other.ext)
					return ext < other.ext ? std::strong_ordering::less : std::strong_ordering::greater;
				return std::strong_ordering::equal;
			}

			bool operator==(const extension& other) const = default;
			bool operator!=(const extension& other) const = default;
			bool operator>(const extension& other) const = default;
			bool operator<(const extension& other) const = default;
			bool operator>=(const extension& other) const = default;
			bool operator<=(const extension& other) const = default;
		};

		class header_pair {
			http_header_key _key;
			http_header_value _value;

			header_pair(http_header_key key, http_header_value value);

		public:
			static header_pair parse(parse_session& session);

			inline const http_header_key& key() const {
				return _key;
			}
			inline const http_header_value& value() const {
				return _value;
			}

			auto operator<=>(const header_pair& other) const {
				if (_key != other._key)
					return _key < other._key ? std::strong_ordering::less : std::strong_ordering::greater;
				if (_value != other._value)
					return _value < other._value ? std::strong_ordering::less : std::strong_ordering::greater;
				return std::strong_ordering::equal;
			}

			bool operator==(const header_pair& other) const = default;
			bool operator!=(const header_pair& other) const = default;
			bool operator>(const header_pair& other) const = default;
			bool operator<(const header_pair& other) const = default;
			bool operator>=(const header_pair& other) const = default;
			bool operator<=(const header_pair& other) const = default;
		};

		struct error_page {
			http_response_code code;
			std::string file;

			static error_page parse(parse_session& session);

			auto operator<=>(const error_page& other) const {
				if (code != other.code)
					return code < other.code ? std::strong_ordering::less : std::strong_ordering::greater;
				if (file != other.file)
					return file < other.file ? std::strong_ordering::less : std::strong_ordering::greater;
				return std::strong_ordering::equal;
			}

			bool operator==(const error_page& other) const = default;
			bool operator!=(const error_page& other) const = default;
			bool operator>(const error_page& other) const = default;
			bool operator<(const error_page& other) const = default;
			bool operator>=(const error_page& other) const = default;
			bool operator<=(const error_page& other) const = default;
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

			auto operator<=>(const server_name& other) const {
				if (name != other.name)
					return name < other.name ? std::strong_ordering::less : std::strong_ordering::greater;
				return std::strong_ordering::equal;
			}

			bool operator==(const server_name& other) const = default;
			bool operator!=(const server_name& other) const = default;
			bool operator>(const server_name& other) const = default;
			bool operator<(const server_name& other) const = default;
			bool operator>=(const server_name& other) const = default;
			bool operator<=(const server_name& other) const = default;
		};

		struct location_filter {
			uri_abs_path path;

			constexpr auto operator<=>(const location_filter& other) const {
				if (path != other.path)
					return path < other.path ? std::strong_ordering::less : std::strong_ordering::greater;
				return std::strong_ordering::equal;
			}

			bool operator==(const location_filter& other) const = default;
			bool operator!=(const location_filter& other) const = default;
			bool operator>(const location_filter& other) const = default;
			bool operator<(const location_filter& other) const = default;
			bool operator>=(const location_filter& other) const = default;
			bool operator<=(const location_filter& other) const = default;

			inline static location_filter parse(parse_session& session) {
				return {fs::path(session.get_word("filter", "filter"))};
			}

			bool starts_with(const location_filter& other) const;
		};

		struct method_filter {
			std::vector<std::string> methods;

			constexpr auto operator<=>(const method_filter& other) const noexcept {
				if (methods != other.methods)
					return methods < other.methods ? std::strong_ordering::less : std::strong_ordering::greater;
				return std::strong_ordering::equal;
			}

			bool operator==(const method_filter& other) const = default;
			bool operator!=(const method_filter& other) const = default;
			bool operator>(const method_filter& other) const = default;
			bool operator<(const method_filter& other) const = default;
			bool operator>=(const method_filter& other) const = default;
			bool operator<=(const method_filter& other) const = default;

			static method_filter parse(parse_session& session);
		};

		class ssl_config {
			config_file _cert;
			config_file _key;

			ssl_config(config_file cert, config_file key);

		public:
			static ssl_config parse(parse_session& session);

			inline const fs::path& cert() const {
				return _cert.file();
			}
			inline const fs::path& key() const {
				return _key.file();
			}

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
			std::optional<
				define<std::variant<static_file_config, cgi_config, fast_cgi_config, redirect_config, proxy_config>>>
				_handler;
			std::vector<std::pair<filter_type, define<block_config>>> _filters;
			std::unordered_map<std::string, file_part> _server_names;
			std::unordered_map<http_response_code, define<error_page>> _error_pages;
			std::set<define<extension>> _extensions;

		public:
			static define<block_config> parse(parse_session& session);

			bool operator==(const block_config& other) const = default;

			friend class config;
			friend class server_config;

		protected:
			void parse_max_body_size(parse_session& session);
			void parse_error_page(parse_session& session);
			void parse_location(parse_session& session);
			void parse_method(parse_session& session);
			void parse_redirect(parse_session& session);
			void parse_static(parse_session& session);
			void parse_proxy(parse_session& session);
			void parse_extension(parse_session& session);
			void parse_cgi(parse_session& session);
			void parse_fast_cgi(parse_session& session);
			void parse_index(parse_session& session);
			void parse_root(parse_session& session);
			void parse_server_name(parse_session& session);
			void parse_set_header(parse_session& session);
			void parse_comment(parse_session& session);

			template <class Container>
			static void throw_undefined_directive(const Container& c, const word& w) {
				throw error(diagnostic::error(w.part(), COBRA_TEXT("unknown directive `{}`", w.str()),
											  COBRA_TEXT("did you mean `{}`?", get_suggestion(c, w.str()))));
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
			static void empty_filters_lint(const std::vector<define<server_config>>& configs,
										   const parse_session& session);
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

				if (config->_handler && !std::holds_alternative<proxy_config>(config->_handler->def) &&
					!config->_root) {
					diagnostic diag = diagnostic::error(config->_handler->part, COBRA_TEXT("unrooted handler"),
														COBRA_TEXT("consider rooting it using: `root`"));
					throw error(diag);
				}

				for (auto& [_, sub_cfg] : config->_filters) {
					unrooted_handler_lint(sub_cfg);
				}
			}

			void parse_listen(parse_session& session);
			void parse_ssl(parse_session& session);
		};

		class config {
		public:
			config* parent;

			std::optional<std::size_t> max_body_size;
			std::optional<fs::path> index;
			std::optional<fs::path> root;
			std::optional<
				std::variant<cobra::static_config, cobra::cgi_config, cobra::redirect_config, cobra::proxy_config>>
				handler;
			/*
			std::optional<
				std::variant<static_file_config, cgi_config, fast_cgi_config, redirect_config, proxy_config>>
				handler;
				*/
			std::unordered_set<http_request_method> methods;
			std::unordered_set<std::string> extensions;
			std::unordered_set<std::string> server_names;
			std::vector<std::shared_ptr<config>> sub_configs;
			std::unordered_map<http_response_code, std::string> error_pages;

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
			return std::format_to(fc.out(), "{}", COBRA_TEXT("error"));
		case diagnostic::level::warning:
			return std::format_to(fc.out(), "{}", COBRA_TEXT("warning"));
		case diagnostic::level::note:
			return std::format_to(fc.out(), "{}", COBRA_TEXT("note"));
		}
		std::terminate();
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
