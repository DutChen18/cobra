#include "cobra/config.hh"

#include "cobra/exception.hh"
#include "cobra/net/address.hh"
#include "cobra/print.hh"

// TODO check which headers aren't needed anymore
#include <algorithm>
#include <any>
#include <cassert>
#include <cctype>
#include <cerrno>
#include <compare>
#include <cstddef>
#include <cstring>
#include <deque>
#include <filesystem>
#include <format>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <ostream>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

namespace cobra {

	namespace config {
		const static std::vector<std::string> BLOCK_KEYWORDS = {
#define X(keyword) #keyword,
			COBRA_BLOCK_KEYWORDS
#undef X
		};

		const static std::vector<std::string> SERVER_KEYWORDS = {
#define X(keyword) #keyword,
			COBRA_SERVER_KEYWORDS
#undef X
		};

		std::size_t levenshtein_dist(const std::string& a, const std::string& b) {
			if (a.length() == 0) {
				return b.length();
			} else if (b.length() == 0) {
				return a.length();
			}

			std::size_t a_len = a.length();
			std::size_t b_len = b.length();

			std::vector dists(a_len, std::vector<std::size_t>(b_len, 0));

			for (std::size_t aidx = 0; aidx < a_len; ++aidx) {
				dists[aidx][0] = aidx;
			}

			for (std::size_t bidx = 0; bidx < b_len; ++bidx) {
				dists[0][bidx] = bidx;
			}

			for (std::size_t bidx = 1; bidx < b_len; ++bidx) {
				for (std::size_t aidx = 1; aidx < a_len; ++aidx) {
					std::size_t sub_cost = 1;
					if (a[aidx] == b[bidx]) {
						sub_cost = 0;
					}

					dists[aidx][bidx] = std::min(
						{dists[aidx - 1][bidx] + 1, dists[aidx][bidx - 1] + 1, dists[aidx - 1][bidx - 1] + sub_cost});
				}
			}
			return dists.back().back();
		}

		constexpr buf_pos::buf_pos(std::size_t line, std::size_t col) noexcept : line(line), col(col) {}

		file_part::file_part(std::optional<fs::path> file, std::size_t line, std::size_t col)
			: file_part(std::move(file), line, col, 1) {}

		file_part::file_part(std::optional<fs::path> file, buf_pos start, buf_pos end)
			: file(std::move(file)), start(start), end(end) {}

		file_part::file_part(std::optional<fs::path> file, std::size_t line, std::size_t col, std::size_t length)
			: file_part(std::move(file), {line, col}, {line, col + length}) {}

		file_part::file_part(std::size_t line, std::size_t col) : file_part(line, col, 1) {}

		file_part::file_part(std::size_t line, std::size_t col, std::size_t length)
			: file_part(std::nullopt, line, col, length) {}

		line_printer::line_printer(std::ostream& stream, std::size_t max_line)
			: _stream(stream), _max_value(max_line), _max_width(std::format("{}", max_line).length()) {}

		line_printer::line_printer(const line_printer& other)
			: _stream(other._stream), _max_value(other._max_value), _max_width(other._max_value) {}

		line_printer::~line_printer() {}

		static std::ostream& print_empty_line_part(std::ostream& out, std::size_t width) {
			cobra::print("{}{} |{} ", term::fg_blue() | term::set_bold(), std::string(width, ' '), term::reset());
			return out;
		}

		static std::ostream& print_line_part(std::ostream& out, std::size_t line_num, std::size_t width) {
			cobra::print("{}{:<{}} |{} ", term::fg_blue() | term::set_bold(), line_num, width, term::reset());
			return out;
		}

		static std::string_view trim_begin(std::string_view view) {
			std::size_t offset = 0;
			for (auto& ch : view) {
				if (!std::isspace(ch))
					break;
				++offset;
			}
			return view.substr(offset);
		}

		line_printer& line_printer::print() {
			print_empty_line_part(_stream, _max_width);
			return *this;
		}

		line_printer& line_printer::println() {
			print_empty_line_part(_stream, _max_width);
			return newline();
		}

		line_printer& line_printer::print(std::size_t line_num) {
			if (line_num > _max_value && std::format("{}", line_num).length() > _max_width) {
				throw std::logic_error("line_printer line_num > max_value");
			}
			print_line_part(_stream, line_num, _max_width);
			return *this;
		}

		line_printer& line_printer::println(std::size_t line_num) {
			print(line_num);
			return newline();
		}

		line_printer& line_printer::newline() {
			cobra::println(_stream, "{}", term::reset());
			return *this;
		}

		multiline_printer::multiline_printer(std::ostream& stream, std::size_t max_line, term::control style,
											 std::span<const std::string> lines)
			: line_printer(stream, max_line), _style(style) {
			std::vector<std::size_t> leading_spaces;

			for (auto& line : lines) {
				std::size_t count = 0;

				for (auto& ch : line) {
					if (!std::isspace(ch))
						break;
					++count;
				}
				leading_spaces.push_back(count);
			}
			_leading_spaces = *std::min_element(leading_spaces.begin(), leading_spaces.end());
		}

		multiline_printer::multiline_printer(const line_printer& printer, term::control style,
											 std::span<const std::string> lines)
			: multiline_printer(printer.stream(), printer.max_value(), style, lines) {}

		multiline_printer& multiline_printer::print() {
			line_printer::print();
			cobra::print(_stream, "{}|{} ", _style, term::reset());
			return *this;
		}

		multiline_printer& multiline_printer::print(std::size_t line) {
			line_printer::print(line);
			cobra::print(_stream, "{}|{} ", _style, term::reset());
			return *this;
		}

		diagnostic::diagnostic(level lvl, file_part part, std::string message) : diagnostic(lvl, part, message, "") {}

		diagnostic::diagnostic(level lvl, file_part part, std::string message, std::string primary_label)
			: diagnostic(lvl, part, message, primary_label, "") {}

		diagnostic::diagnostic(level lvl, file_part part, std::string message, std::string primary_label,
							   std::string secondary_label)
			: lvl(lvl), part(std::move(part)), message(std::move(message)), primary_label(std::move(primary_label)),
			  secondary_label(std::move(secondary_label)) {}

		std::ostream& diagnostic::print_single(std::ostream& out, const std::vector<std::string>& lines) const {
			const std::size_t line_num = part.start.line + 1;
			const auto line = lines.at(line_num - 1);
			const auto trimmed_line = trim_begin(line);

			line_printer printer(out, line_num);

			printer.println().println(line_num, "{}", config_line{trimmed_line});
			print_single_diag(printer, part.start.col - line.length() + trimmed_line.length());
			return out;
		}

		void diagnostic::print_single_diag(line_printer& printer, std::size_t offset) const {
			const std::size_t length = part.end.col - part.start.col;
			const auto style = get_color(lvl) | term::set_bold();
			const std::string spacing = std::string(offset, ' ');

			printer.println("{}{}{} {}", spacing, style, std::string(length, '^'), secondary_label);
			if (!primary_label.empty()) {
				printer.println("{}{}|", spacing, style);
				printer.println("{}{}{}", spacing, style, primary_label);
			}
		}

		std::ostream& diagnostic::print_multi(std::ostream& out, const std::vector<std::string>& lines,
											  std::deque<std::reference_wrapper<const diagnostic>> inline_diags) const {
			const std::size_t start_line = part.start.line;
			const std::size_t start_col = part.start.col;
			const std::size_t end_line = part.end.line;
			const std::size_t end_col = part.end.col;

			std::span diag_lines = std::span(lines.begin() + start_line, lines.begin() + end_line + 1);
			std::vector<std::size_t> leading_spaces;

			for (auto line : diag_lines) {
				std::size_t count = 0;

				for (auto ch : line) {
					if (!std::isspace(ch))
						break;
					++count;
				}
				leading_spaces.push_back(count);
			}

			std::size_t min_leading = *std::min_element(leading_spaces.begin(), leading_spaces.end());

			std::size_t start_offset = 1;
			std::size_t end_offset = end_line - start_line;

			const auto style = get_color(lvl) | term::set_bold();

			line_printer printer(out, end_line);
			multiline_printer mprinter(printer, style, diag_lines);

			if (start_col <= leading_spaces.front()) {
				auto line = diag_lines.front().substr(min_leading);
				printer.println(start_line, "{}/{} {}", style, term::reset(), config_line{line});
			} else {
				printer.println("{} {}v", style, std::string(start_col - min_leading + 1, '_'));
				start_offset = 0;
			}

			if (end_col > leading_spaces.back() || !primary_label.empty()) {
				end_offset += 1;
			}

			for (std::size_t offset = start_offset; offset < end_offset; ++offset) {
				const auto line = lines[start_line + offset].substr(min_leading);
				mprinter.println(start_line + offset, "{}", config_line{line});
				// printer.println(start_line + offset, "{}|{} {}", style, term::reset(), config_line{line});

				if (!inline_diags.empty() && inline_diags.front().get().part.start.line == start_line + offset) {
					const diagnostic& diag = inline_diags.front();
					diag.print_single_diag(mprinter, lines[start_line + offset].length() - line.length() + 1);
					inline_diags.pop_front();
				}
			}

			if (primary_label.empty()) {
				if (end_col <= leading_spaces.back()) {
					auto line = diag_lines.back().substr(min_leading);
					printer.println(end_line, "{}\\{} {}", style, term::reset(), config_line{line});
				} else {
					printer.println("{}|{}^", style, std::string(end_col - min_leading + 2, '_'));
				}
			} else {
				printer.println("{}|{}^ {}", style, std::string(end_col - min_leading + 1, '_'), primary_label);
			}
			return out;
		}

		std::ostream& diagnostic::print(std::ostream& out, const std::vector<std::string>& lines) const {
			const auto style = get_color(lvl) | term::set_bold();
			cobra::println(out, "{}{}{}{}: {}{}", style, lvl, term::reset(), term::set_bold(), message, term::reset());

			std::string file("<source>");
			if (part.file) {
				file = part.file->string();
			}
			cobra::println(out, " {}-->{} {}:{}:{}", term::fg_blue() | term::set_bold(), term::reset(), file,
						   part.start.line + 1, part.start.col + 1);

			std::vector<std::reference_wrapper<const diagnostic>> sub_diags;

			if (part.is_multiline()) {
				std::deque<std::reference_wrapper<const diagnostic>> inline_diags;

				for (auto& diag : this->sub_diags) {
					if (!diag.part.is_multiline()) {
						std::size_t line = diag.part.start.line;
						if (line > part.start.line && line < part.end.line) {
							inline_diags.push_back(diag);
						}
					} else {
						sub_diags.push_back(diag);
					}
				}

				print_multi(out, lines, std::move(inline_diags));
			} else {
				print_single(out, lines);

				for (auto& diag : this->sub_diags) {
					sub_diags.push_back(diag);
				}
			}

			for (auto& diag : sub_diags) {
				diag.get().print(out, lines);
			}
			return out;
		}

		term::control diagnostic::get_color(level lvl) {
			switch (lvl) {
			case diagnostic::level::error:
				return term::fg_red();
			case diagnostic::level::warning:
				return term::fg_yellow();
			case diagnostic::level::note:
				return term::fg_white() | term::set_faint();
			}
		}

		std::ostream& operator<<(std::ostream& stream, diagnostic::level lvl) {
			switch (lvl) {
			case diagnostic::level::error:
				return stream << "error";
			case diagnostic::level::warning:
				return stream << "warning";
			case diagnostic::level::note:
				return stream << "note";
			}
		}

		error::error(diagnostic diag) : std::runtime_error(diag.message), _diag(diag) {}

		basic_diagnostic_reporter::basic_diagnostic_reporter(bool print) : _print(print) {}

		void basic_diagnostic_reporter::report(const diagnostic& diag, const parse_session& session) {
			if (_print) {
				diag.print(std::cerr, session.lines());
			}

			_diags.push_back(diag);
		}

		void basic_diagnostic_reporter::report_token(file_part part, std::string type, const parse_session& session) {
			(void)session;
			_tokens.push_back({part, std::move(type)});
		}

		void basic_diagnostic_reporter::report_inlay_hint(buf_pos pos, std::string hint, const parse_session& session) {
			(void)session;
			_inlay_hints.push_back({pos, std::move(hint)});
		}

		word::word(file_part part, std::string str) : _part(std::move(part)), _str(std::move(str)) {}

		parse_session::parse_session(std::istream& stream, diagnostic_reporter& reporter)
			: _stream(stream), _lines(), _reporter(&reporter), _file(), _col_num(0) {
			_stream.exceptions(std::ios::badbit);
		}

		std::optional<int> parse_session::peek() {
			const std::string_view buf = fill_buf();
			if (!buf.empty())
				return buf[0];
			return std::nullopt;
		}

		std::optional<int> parse_session::get() {
			const auto res = peek();
			if (res)
				consume(1);
			return res;
		}

		void parse_session::expect(int ch) {
			const std::size_t col = column();
			const auto got = get();

			if (!got) {
				throw error(
					make_error(line(), col, "unexpected EOF", std::format("expected `{}`", static_cast<char>(ch))));
			} else if (got != ch) {
				throw error(make_error(line(), col, std::format("unexpected `{}`", static_cast<char>(*got)),
									   std::format("expected `{}`", static_cast<char>(ch))));
			}
		}

		void parse_session::expect_word_simple(std::string_view str) {
			const std::size_t col_num = column();
			const std::size_t line_num = line();

			const std::string word = get_word_simple();

			if (word != str) {
				throw error(make_error(line_num, col_num, "unexpected word", std::format("expected `{}`", str),
									   std::format("consider replacing `{}` with `{}`", word, str)));
			}
		}

		word parse_session::get_word() {
			if (peek() == '"')
				return get_word_quoted();
			return get_word_simple();
		}

		std::size_t parse_session::ignore_ws() {
			std::size_t nignored = 0;

			while (true) {
				const auto ch = peek();

				if (!ch || !std::isspace(*ch))
					break;
				consume(1);
			}
			return nignored;
		}

		std::size_t parse_session::ignore_line() {
			return consume(remaining().length());
		}

		word parse_session::get_word_quoted() {
			const std::size_t start_col = column();
			const std::size_t start_line = line();
			std::size_t end_col = start_col;
			std::size_t end_line = start_line;
			consume(1);

			std::string res;

			bool escaped = false;
			while (true) {
				end_col = column();
				end_line = line();
				const auto ch = get();

				if (ch == '\\') {
					escaped = true;
				} else if (ch == '"' && !escaped) {
					break;
				} else if (ch) {
					if (escaped && ch != '"') {
						res.push_back('\\');
						escaped = false;
					}

					res.push_back(*ch);
				} else {
					throw error(make_error(start_line, start_col, "unclosed quote"));
				}
			}

			if (res.empty())
				throw error(make_error(start_line, start_col, 2, "invalid word", "expected at least one character"));
			return word(file_part(file(), buf_pos(start_line, start_col), buf_pos(end_line, end_col)), std::move(res));
		}

		word parse_session::get_word_simple() {
			std::size_t start_line = line();
			std::size_t start_col = column();
			std::string res;

			for (auto ch : remaining()) {
				if (!std::isgraph(ch))
					break;
				res.push_back(ch);
			}

			if (res.empty())
				throw error(make_error("invalid word", "expected at least one graphical character"));
			consume(res.length());
			return word(file_part(file(), start_line, start_col, res.length()), std::move(res));
		}

		void parse_session::expect_word_simple(std::string_view str, std::string type) {
			std::size_t start_line = line();
			std::size_t start_col = column();
			expect_word_simple(str);
			report_token(file_part(file(), start_line, start_col, str.size()), std::move(type));
		}

		word parse_session::get_word(std::string type) {
			word w = get_word();
			report_token(w.part(), std::move(type));
			return w;
		}

		word parse_session::get_word_simple(std::string type) {
			word result = get_word_simple();
			report_token(result.part(), std::move(type));
			return result;
		}

		void parse_session::expect_word_simple(std::string_view str, std::string type, std::string hint) {
			std::size_t start_line = line();
			std::size_t start_col = column();
			expect_word_simple(str);
			report_token(file_part(file(), start_line, start_col, str.size()), std::move(type));
			report_inlay_hint(buf_pos(start_line, start_col), std::move(hint));
		}

		word parse_session::get_word(std::string type, std::string hint) {
			word result = get_word();
			report_token(result.part(), std::move(type));
			report_inlay_hint(result.part().start, std::move(hint));
			return result;
		}

		word parse_session::get_word_simple(std::string type, std::string hint) {
			word result = get_word_simple();
			report_token(result.part(), std::move(type));
			report_inlay_hint(result.part().start, std::move(hint));
			return result;
		}

		std::optional<std::string> parse_session::read_line() {
			std::string line;

			std::getline(_stream, line);
			if (_stream.fail() || (line.empty() && _stream.eof()))
				return std::nullopt;
			return line;
		}

		std::string_view parse_session::fill_buf() {
			if (_lines.size() == 0) {
				auto line = read_line();

				if (line) {
					_lines.push_back(std::move(*line));
					return fill_buf();
				}
				return std::string_view();
			} else if (!remaining().empty()) {
				return remaining();
			} else {
				auto line = read_line();

				if (!line)
					return std::string_view();

				_col_num = 0;

				_lines.push_back(std::move(*line));
				return fill_buf();
			}
		}

		std::size_t parse_session::consume(std::size_t count) {
			assert(remaining().length() >= count && "tried to consume more than available");
			_col_num += count;
			return count;
		}

		std::string_view parse_session::get_line(std::size_t idx) const {
			return _lines.at(idx);
		}

		bool location_filter::starts_with(const location_filter& other) const {
			if (other.path.size() > path.size())
				return false;

			auto ita = path.begin();
			auto itb = other.path.begin();

			while (itb != other.path.end()) {
				if (itb + 1 == other.path.end())
					return ita->starts_with(*itb);
				if (*ita != *itb)
					return false;
				++ita;
				++itb;
			}
			return true;
		}

		constexpr listen_address::listen_address(std::string node, port service) noexcept
			: _node(std::move(node)), _service(service) {}
		constexpr listen_address::listen_address(port service) noexcept : _node(), _service(service) {}

		static const char* get_filetype_name(fs::file_type type) {
			switch (type) {
			case fs::file_type::regular:
				return "regular file";
			case fs::file_type::directory:
				return "directory";
			case fs::file_type::symlink:
				return "symlink";
			case fs::file_type::block:
				return "block device";
			case fs::file_type::character:
				return "character device";
			case fs::file_type::fifo:
				return "fifo";
			case fs::file_type::socket:
				return "socket";
			default:
				return "unknown";
			}
		}

		config_file::config_file(fs::path file) : _file(std::move(file)) {}

		static void report_fs_error(file_part part, const parse_session& session, const fs::filesystem_error& ex) {
			if (ex.code().value() != EACCES) {
				session.report(diagnostic::warn(part, "failed to stat file", ex.code().message()));
			}
		}

		config_file config_file::parse(parse_session& session) {
			word w = session.get_word("string", "path");
			fs::path p(w);

			if (p.is_absolute()) {
				try {
					fs::file_status stat = fs::status(p);

					if (stat.type() == fs::file_type::directory) {
						session.report(diagnostic::warn(w.part(), "not a normal file", "is a directory"));
					}
				} catch (const fs::filesystem_error& ex) {
					report_fs_error(w.part(), session, ex);
				}
			}

			return config_file{std::move(p)};
		}

		config_exec::config_exec(fs::path file) : config_file(std::move(file)) {}

		config_exec config_exec::parse(parse_session& session) {
			word w = session.get_word("string", "path");
			fs::path p(w);

			if (p.is_absolute()) {
				try {
					fs::file_status stat = fs::status(p);
					fs::perms perms = stat.permissions();

					if (stat.type() == fs::file_type::directory) {
						session.report(diagnostic::warn(w.part(), "not a normal file", "is a directory"));
					}

					if ((perms & fs::perms::owner_exec) == fs::perms::none &&
						(perms & fs::perms::group_exec) == fs::perms::none &&
						(perms & fs::perms::others_exec) == fs::perms::none) {
						session.report(diagnostic::warn(w.part(), "not an executable file"));
					}
				} catch (const fs::filesystem_error& ex) {
					report_fs_error(w.part(), session, ex);
				}
			}
			return config_exec(std::move(p));
		}

		config_dir::config_dir(fs::path dir) : _dir(std::move(dir)) {}

		config_dir config_dir::parse(parse_session& session) {
			word w = session.get_word("string", "path");
			fs::path p(w);

			if (p.is_absolute()) {
				try {
					fs::file_status stat = fs::status(p);

					if (stat.type() != fs::file_type::directory) {
						session.report(diagnostic::warn(w.part(), "not a directory",
														std::format("is a {}", get_filetype_name(stat.type()))));
					}
				} catch (const fs::filesystem_error& ex) {
					report_fs_error(w.part(), session, ex);
				}
			}

			return config_dir{std::move(p)};
		}

		define<server_config> server_config::parse(parse_session& session) {
			const std::size_t start_line = session.line();
			const std::size_t start_col = session.column() - std::string_view("server").length();
			session.ignore_ws();
			session.expect('{');

			server_config config;

			std::size_t end_line, end_col;

			while (true) {
				session.ignore_ws();

				if (session.peek() == '}') {
					end_line = session.line();
					end_col = session.column();
					session.get();
					break;
				} else if (session.peek() == '#') {
					config.parse_comment(session);
					continue;
				}

				const word w = session.get_word_simple("keyword");

#define X(name)                                                                                                        \
	if (w.str() == #name) {                                                                                            \
		config.parse_##name(session);                                                                                  \
		continue;                                                                                                      \
	}
				COBRA_SERVER_KEYWORDS
#undef X

				throw_undefined_directive(SERVER_KEYWORDS, w);
			}

			return define<server_config>(std::move(config), file_part(session.file(), buf_pos(start_line, start_col),
																	  buf_pos(end_line, end_col)));
		}

		std::vector<server_config> server_config::parse_servers(parse_session& session) {
			std::vector<define<server_config>> configs;

			while (true) {
				session.ignore_ws();

				if (session.eof())
					break;

				session.expect_word_simple("server", "keyword");

				configs.push_back(server_config::parse(session));
			}

			server_config::lint_configs(configs, session);

			std::vector<server_config> result;
			result.reserve(configs.size());
			for (auto&& cfg : configs) {
				result.push_back(std::move(cfg));
			}
			return result;
		}

		void server_config::lint_configs(const std::vector<define<server_config>>& configs,
										 const parse_session& session) {
			empty_filters_lint(configs, session);
			ssl_lint(configs);
			server_name_lint(configs);
			(void)session;
		}

		void server_config::empty_filters_lint(const std::vector<define<server_config>>& configs,
											   const parse_session& session) {
			for (auto& cfg : configs) {
				for (auto& [_, block_cfg] : cfg->_filters) {
					empty_filters_lint(block_cfg, session);
				}
			}
		}

		void server_config::empty_filters_lint(const define<block_config>& config, const parse_session& session) {
			if (!config->_handler && config->_filters.empty()) {
				diagnostic diag = diagnostic::warn(config.part, "empty filter",
												   "consider specifying a handler using: `root`, `cgi`, etc...");
				session.report(diag);
			} else {
				for (auto& [_, block_cfg] : config->_filters) {
					empty_filters_lint(block_cfg, session);
				}
			}
		}

		void server_config::ssl_lint(const std::vector<define<server_config>>& configs) {
			std::map<listen_address, file_part> plain_ports;
			std::map<listen_address, file_part> ssl_ports;

			for (auto& define : configs) {
				const auto& config = define.def;
				if (config._ssl) {
					for (auto& address : config._addresses) {
						const auto it = plain_ports.find(address);
						if (it != plain_ports.end()) {
							diagnostic diag = diagnostic::error(config._ssl->part,
																"a non-ssl address cannot be listened to using ssl",
																"consider listening to another address");
							diag.sub_diags.push_back(
								diagnostic::note(it->second, "previously listened to here without ssl"));
							throw error(diag);
						}
						ssl_ports.insert({address, config._ssl->part});
					}
				} else {
					for (auto& address : config._addresses) {
						const auto it = ssl_ports.find(address);
						if (it != ssl_ports.end()) {
							diagnostic diag =
								diagnostic::error(config._ssl->part, "a ssl address cannot be listened without ssl",
												  "consider listening to another address");
							diag.sub_diags.push_back(
								diagnostic::note(it->second, "previously listened to here with ssl"));
							throw error(diag);
						}
						plain_ports.insert({address, address.part});
					}
				}
			}
		}

		void server_config::server_name_lint(const std::vector<define<server_config>>& defines) {
			std::map<listen_address, std::vector<std::reference_wrapper<const define<server_config>>>> servers;

			for (const auto& define : defines) {
				for (auto& address : define.def._addresses) {
					servers[address].push_back(define);
				}
			}

			for (const auto& [address, defines] : servers) {
				if (defines.size() < 2)
					continue;
				std::unordered_map<std::string_view, file_part> names;

				for (const auto& define : defines) {
					const auto& config = define.get().def;
					if (config._server_names.empty()) {
						diagnostic diag = diagnostic::error(define.get().part, "ambigious server",
															"consider specifying a `server_name`");
						auto address_define =
							std::find_if(config._addresses.begin(), config._addresses.end(), [address](auto addr) {
								return addr == address;
							});

						diag.sub_diags.push_back(
							diagnostic::error(address_define->part, "ambigious server", "",
											  std::format("another server is also listening to `{}`", address)));

						for (const auto& ambigious_define : defines) {
							if (ambigious_define.get().part == define.get().part)
								continue;
							diagnostic sub_diag =
								diagnostic::note(ambigious_define.get().part, "also listened to here");

							auto other_address_define =
								std::find_if(ambigious_define.get().def._addresses.begin(),
											 ambigious_define.get().def._addresses.end(), [address](auto addr) {
												 return addr == address;
											 });
							sub_diag.sub_diags.push_back(
								diagnostic::note(other_address_define->part, "also listened to here"));
							diag.sub_diags.push_back(std::move(sub_diag));
						}

						throw error(diag);
					}

					for (auto& [name, part] : config._server_names) {
						auto [it, inserted] = names.insert({name, part});

						if (!inserted) {
							diagnostic diag = diagnostic::error(
								part, "ambigious server",
								"another server listening to the same address has the same `server_name`");
							diag.sub_diags.push_back(diagnostic::note(it->second, "previously specified here"));
							throw error(diag);
						}
					}
				}
			}
		}

		listen_address listen_address::parse(parse_session& session) {
			session.ignore_ws();

			word w = session.get_word_simple("string", "address");
			std::size_t port_start = w.part().start.col;
			std::size_t port_length = w.str().length();

			auto pos = w.str().find(':');

			std::string node;

			if (pos != std::string::npos) {
				node = w.str().substr(0, pos);
				port_start += pos + 1;
				port_length -= pos + 1;
				pos += 1;
			} else {
				pos = 0;
			}

			port p;

			std::string port_str = w.str().substr(pos);

			try {
				p = parse_unsigned<port>(port_str);
			} catch (error err) {
				err.diag().message = "invalid port";
				err.diag().part = file_part(session.file(), w.part().start.line, port_start, port_length);
				throw err;
			}

			return listen_address(std::move(node), p);
		}

		std::strong_ordering filter::operator<=>(const filter& other) const {
			if (type == other.type)
				return match <=> other.match;
			return type <=> other.type;
		}

		ssl_config::ssl_config(config_file cert, config_file key) : _cert(std::move(cert)), _key(std::move(key)) {}

		ssl_config ssl_config::parse(parse_session& session) {
			config_file cert = config_file::parse(session);
			session.ignore_ws();
			config_file key = config_file::parse(session);
			return {std::move(cert), std::move(key)};
		}

		define<block_config> block_config::parse(parse_session& session) {
			session.ignore_ws();
			const std::size_t start_line = session.line();
			const std::size_t start_col = session.column();
			session.expect('{');

			block_config config;

			std::size_t end_line, end_col;

			while (true) {
				session.ignore_ws();

				if (session.peek() == '}') {
					end_line = session.line();
					end_col = session.column();
					session.get();
					break;
				} else if (session.peek() == '#') {
					config.parse_comment(session);
					continue;
				}

				const word w = session.get_word_simple("keyword");

#define X(name)                                                                                                        \
	if (w.str() == #name) {                                                                                            \
		config.parse_##name(session);                                                                                  \
		continue;                                                                                                      \
	}
				COBRA_BLOCK_KEYWORDS
#undef X
				throw_undefined_directive(BLOCK_KEYWORDS, w);
			}
			return define<block_config>(std::move(config), file_part(session.file(), buf_pos(start_line, start_col),
																	 buf_pos(end_line, end_col)));
		}

		void block_config::parse_max_body_size(parse_session& session) {
			const std::size_t define_start = session.column() - std::string("max_body_size").length();
			session.ignore_ws();

			const std::size_t col = session.column();
			const std::size_t line = session.line();

			std::string word;
			try {
				word = session.get_word_simple("number", "size");
			} catch (error err) {
				err.diag().message = "invalid number";
				throw err;
			}

			try {
				const std::size_t len = col + word.length() - define_start;
				const std::size_t limit = parse_unsigned<std::size_t>(word);

				if (_max_body_size) {
					diagnostic diag = session.make_warn(line, define_start, len, "redefinition of max_body_size");
					diag.sub_diags.push_back(diagnostic::note(_max_body_size->part, "previously defined here"));
					session.report(diag);
				}

				_max_body_size = define<std::size_t>(
					limit, file_part(session.file(), line, define_start, len)); // TODO use parse_define
			} catch (error err) {
				err.diag().message = "invalid max_body_size";
				err.diag().part = file_part(session.file(), line, col, word.length());
				throw err;
			}
		}

		void block_config::parse_location(parse_session& session) {
			auto def = parse_define<location_filter>(session, "location");
			session.ignore_ws();

			auto block = block_config::parse(session);
			block.part.start = def.part.start;
			// define<block_config> block = define<block_config>{block_config::parse(session), std::move(def.part)};
			block->_filter = def;

			auto it = std::find_if(_filters.begin(), _filters.end(), [&def](auto pair) {
				if (!std::holds_alternative<location_filter>(pair.first))
					return false;
				const location_filter& other = std::get<location_filter>(pair.first);

				return def->starts_with(other);
			});

			if (it != _filters.end()) {
				diagnostic diag = diagnostic::warn(block.part, "unreachable filter");
				if (std::get<location_filter>(it->first) == def.def) {
					diag.sub_diags.push_back(
						diagnostic::note(it->second.part, "because of an earlier definition here"));
				} else {
					diag.sub_diags.push_back(
						diagnostic::note(it->second.part, "because a less strict filter defined earlier here"));
				}
				session.report(diag);
			}

			_filters.push_back({std::move(def.def), std::move(block)});
		}

		void block_config::parse_cgi(parse_session& session) {
			define<cgi_config> def = parse_define<cgi_config>(session, "cgi");
			if (_handler) { // TODO dry
				diagnostic diag = diagnostic::warn(def.part, "redefinition of request handler");
				diag.sub_diags.push_back(diagnostic::note(_handler->part, "previously defined here"));
				session.report(diag);
			}
			_handler = std::move(def);
		}

		void block_config::parse_root(parse_session& session) {
			define<static_file_config> def = parse_define<static_file_config>(session, "root");
			if (_handler) {
				diagnostic diag = diagnostic::warn(def.part, "redefinition of request handler");
				diag.sub_diags.push_back(diagnostic::note(_handler->part, "previously defined here"));
				session.report(diag);
			}
			_handler = std::move(def);
		}

		void block_config::parse_index(parse_session& session) {
			auto def = parse_define<config_file>(session, "index");
			if (_index) {
				diagnostic diag = diagnostic::warn(def.part, "redefinition of index");
				diag.sub_diags.push_back(diagnostic::note(_index->part, "previously defined here"));
				session.report(diag);
			}
			_index = std::move(def);
		}

		void server_config::parse_listen(parse_session& session) {
			auto def = parse_define<listen_address>(session, "listen");

			auto [it, inserted] = _addresses.insert(def);
			if (!inserted) {
				diagnostic diag = diagnostic::warn(def.part, "duplicate listen");
				diag.sub_diags.push_back(diagnostic::note(it->part, "previously defined here"));
				session.report(diag);
			}
		}

		void block_config::parse_server_name(parse_session& session) {
			auto def = parse_define<server_name>(session, "server_name");

			auto [it, inserted] = _server_names.insert({def.def.name, def.part});
			if (!inserted) {
				diagnostic diag = diagnostic::warn(def.part, "duplicate server_name");
				diag.sub_diags.push_back(diagnostic::note(it->second, "previously defined here"));
				session.report(diag);
			}
		}

		void block_config::parse_comment(parse_session& session) {
			size_t start_line = session.line();
			size_t start_col = session.column();
			size_t length = session.ignore_line();
			session.report_token(file_part(session.file(), start_line, start_col, length), "comment");
		}

		void server_config::parse_ssl(parse_session& session) {
			_ssl = parse_define<ssl_config>(session, "ssl");
		}

		config::config(config* parent, const block_config& cfg)
			: parent(parent), max_body_size(cfg._max_body_size), handler(cfg._handler) {
			if (cfg._index)
				index = cfg._index->def.file();

			for (auto [name, _] : cfg._server_names) {
				server_names.insert(name);
			}

			if (cfg._filter) {
				if (std::holds_alternative<location_filter>(*cfg._filter)) {
					location = std::get<location_filter>(*cfg._filter).path;
				} else if (std::holds_alternative<method_filter>(*cfg._filter)) {
					methods.insert(std::get<method_filter>(*cfg._filter).method);
				} else {
					assert(0 && "filter not implemented");
				}
			}

			for (auto& [filter, sub_cfg] : cfg._filters) {
				sub_configs.push_back(std::shared_ptr<config>(new config(this, sub_cfg)));
			}

			if (parent) {
				if (!index)
					index = parent->index;
				if (!max_body_size)
					max_body_size = parent->max_body_size;
				if (!handler)
					handler = parent->handler;
				if (server_names.empty())
					server_names = parent->server_names;
			}
		}

		config::config(const block_config& cfg) : config(nullptr, cfg) {}
		config::~config() {}

		server::server(const server_config& cfg) : config(cfg), ssl(cfg._ssl) {
			for (auto& address : cfg._addresses) {
				addresses.push_back(address);
			}
		}
	} // namespace config

} // namespace cobra
