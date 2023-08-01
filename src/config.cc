#include "cobra/config.hh"

#include "cobra/exception.hh"
#include "cobra/print.hh"
#include "cobra/net/address.hh"

//TODO check which headers aren't needed anymore
#include <algorithm>
#include <any>
#include <cctype>
#include <cerrno>
#include <compare>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <ostream>
#include <ranges>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <format>
#include <memory>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>
#include <cassert>

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

					dists[aidx][bidx] = std::min({dists[aidx-1][bidx] + 1, dists[aidx][bidx-1] + 1, dists[aidx -1][bidx -1] + sub_cost});
				}
			}
			return dists.back().back();
		}

		constexpr buf_pos::buf_pos(std::size_t line, std::size_t col) noexcept : line(line), col(col) {}

		file_part::file_part(std::optional<fs::path> file, std::size_t line, std::size_t col)
			: file_part(std::move(file), line, col, 1) {}

		file_part::file_part(std::optional<fs::path> file, buf_pos start, buf_pos end) : file(std::move(file)), start(start), end(end) {}

		file_part::file_part(std::optional<fs::path> file, std::size_t line, std::size_t col, std::size_t length)
			: file_part(std::move(file), {line, col}, {line, col + length}) {}

		file_part::file_part(std::size_t line, std::size_t col) : file_part(line, col, 1) {}

		file_part::file_part(std::size_t line, std::size_t col, std::size_t length)
			: file_part(std::nullopt, line, col, length) {}

		diagnostic::diagnostic(level lvl, file_part part, std::string message) : diagnostic(lvl, part, message, "") {}

		diagnostic::diagnostic(level lvl, file_part part, std::string message, std::string primary_label)
			: diagnostic(lvl, part, message, primary_label, "") {}

		diagnostic::diagnostic(level lvl, file_part part, std::string message, std::string primary_label,
							   std::string secondary_label)
			: lvl(lvl), part(std::move(part)), message(std::move(message)), primary_label(std::move(primary_label)),
			  secondary_label(std::move(secondary_label)) {}

		std::ostream& diagnostic::print_single(std::ostream& out, const std::vector<std::string>& lines) const {
			const std::size_t line_num = part.start.line;
			const std::string line = std::format("{}", line_num + 1);

			cobra::println(out, "{}{} |{} ", term::fg_blue() | term::set_bold(), std::string(line.length(), ' '),
						   term::reset());
			cobra::print(out, "{}{} |{} ", term::fg_blue() | term::set_bold(), line, term::reset());

			for (auto&& ch : lines.at(line_num)) {
				if (ch == '\t') {
					out << ' ';
				} else {
					out << ch;
				}
			}

			const std::size_t col = part.start.col;
			const std::size_t length = part.end.col - part.start.col;

			out << std::endl;
			cobra::println(out, "{}{} |{} {}{}{}{}{}", term::fg_blue() | term::set_bold(),
						   std::string(line.length(), ' '), term::reset(), std::string(col, ' '),
						   get_control(lvl) | term::set_bold(), std::string(length, '^'), term::reset(),
						   secondary_label);
			if (!primary_label.empty()) {
				cobra::println(out, "{}{} |{} {}{}|{}", term::fg_blue() | term::set_bold(),
							   std::string(line.length(), ' '), term::reset(), std::string(col, ' '),
							   get_control(lvl) | term::set_bold(), term::reset());
				cobra::println(out, "{}{} |{} {}{}{}{}", term::fg_blue() | term::set_bold(),
							   std::string(line.length(), ' '), term::reset(), std::string(col, ' '),
							   get_control(lvl) | term::set_bold(), primary_label, term::reset());
			}
			return out;
		}

		std::ostream& diagnostic::print_multi(std::ostream& out, const std::vector<std::string>& lines) const {
                        std::vector<std::size_t> leading_spaces;

                        const std::size_t start_line = part.start.line;
                        const std::size_t end_line = part.end.line;

                        for (auto line : std::span(lines.begin() + start_line, lines.begin() + end_line)) {
                                std::size_t count = 0;

                                for (auto ch : line) {
                                        if (!std::isspace(ch))
                                                break;
                                        ++count;
                                }
                                leading_spaces.push_back(count);
                        }

                        std::size_t min_leading = *std::min_element(leading_spaces.begin(), leading_spaces.end());

			const std::size_t line_length = std::format("{}", end_line).length();
			const std::size_t max_col_length = 80;

                        eprintln("leading spaces[0]: {}, min: {}", leading_spaces[0], min_leading);
                        cobra::println(
                            out, "{}{:{}} |{} {}{}{}{}",
                            term::fg_blue() | term::set_bold(), start_line,
                            line_length, term::reset(),
                            get_control(lvl) | term::set_bold(),
                            std::string(leading_spaces[0] - min_leading + 1, '-'),
                            term::reset(),
                            std::string_view(lines[start_line].begin() +
                                                 leading_spaces[0],
                                             lines[start_line].end()));

                        for (std::size_t offset = 1; offset < (end_line - start_line); ++offset) {
                                const std::string& line = lines[start_line + offset];
                                cobra::println(
                                    out, "{}{:{}} |{} {}{}{}{}",
                                    term::fg_blue() | term::set_bold(),
                                    start_line + offset, line_length,
                                    term::reset(),
                                    get_control(lvl) | term::set_bold(), "|",
                                    term::reset(),
                                    std::string_view(line.begin() + min_leading,
                                                     line.end()));
                        }

                        cobra::println(
                            out, "{}{:{}} |{} {}{}{}{}",
                            term::fg_blue() | term::set_bold(), end_line,
                            line_length, term::reset(),
                            get_control(lvl) | term::set_bold(),
                            std::string(leading_spaces.back() - min_leading + 1, '-'),
                            term::reset(),
                            std::string_view(lines[end_line].begin() +
                                                 leading_spaces.back(),
                                             lines[end_line].end()));
			return out;
                }

		std::ostream& diagnostic::print(std::ostream& out, const std::vector<std::string>& lines) const {
			cobra::println(out, "{}{}{}{}: {}{}", get_control(lvl) | term::set_bold(), lvl, term::reset(), term::set_bold(), message,
						   term::reset());

			std::string file("<source>");
			if (part.file) {
				file = part.file->string();
			}
			cobra::println(out, " {}-->{} {}:{}:{}", term::fg_blue() | term::set_bold(), term::reset(), file,
						   part.start.line + 1, part.start.col + 1);

			if (part.is_multiline()) {
				print_multi(out, lines);
			} else {
				print_single(out, lines);
			}

			for (auto&& diag : sub_diags) {
				diag.print(out, lines);
			}
			return out;
		}

		term::control diagnostic::get_control(level lvl) {
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
		
		basic_diagnostic_reporter::basic_diagnostic_reporter(bool print) : _print(print) {
		}

		void basic_diagnostic_reporter::report(const diagnostic& diag, const parse_session& session) {
			if (_print) {
				diag.print(std::cerr, session.lines());
			}

			_diags.push_back(diag);
		}
		
		void basic_diagnostic_reporter::report_token(file_part part, std::string type, const parse_session& session) {
			(void) session;
			_tokens.push_back({ part, std::move(type) });
		}
		
		void basic_diagnostic_reporter::report_inlay_hint(buf_pos pos, std::string hint, const parse_session& session) {
			(void) session;
			_inlay_hints.push_back({ pos, std::move(hint) });
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

		constexpr listen_address::listen_address(std::string node, port service) noexcept : _node(std::move(node)), _service(service) {}
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

                static void report_fs_error(file_part part, const parse_session& session, const fs::filesystem_error &ex) {
                        if (ex.code().value() != EACCES) {
                                session.report(diagnostic::warn(
                                    part, "failed to stat file",
                                    ex.code().message()));
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
                                } catch (const fs::filesystem_error &ex) {
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
                                                session.report(diagnostic::warn(
                                                    w.part(),
                                                    "not a normal file",
                                                    "is a directory"));
                                        }

                                        if ((perms & fs::perms::owner_exec) ==
                                                fs::perms::none &&
                                            (perms & fs::perms::group_exec) ==
                                                fs::perms::none &&
                                            (perms & fs::perms::others_exec) ==
                                                fs::perms::none) {
                                                session.report(diagnostic::warn(
                                                    w.part(),
                                                    "not an executable file"));
                                        }
                                } catch (const fs::filesystem_error &ex) {
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
                                                session.report(diagnostic::warn(
                                                    w.part(), "not a directory",
                                                    std::format(
                                                        "is a {}",
                                                        get_filetype_name(
                                                            stat.type()))));
                                        }
                                } catch (const fs::filesystem_error &ex) {
                                        report_fs_error(w.part(), session, ex);
                                }
                        }

                        return config_dir{std::move(p)};
		}

		server_config server_config::parse(parse_session& session) {
			session.ignore_ws();
			session.expect('{');

			server_config config;

			while (true) {
				session.ignore_ws();

				if (session.peek() == '}') {
					session.get();
					break;
				} else if (session.peek() == '#') {
					config.parse_comment(session);
					continue;
				}

				const word w = session.get_word_simple("keyword");

#define X(name) \
				if (w.str() == #name) {\
					config.parse_##name(session);\
					continue;\
				}
				COBRA_SERVER_KEYWORDS
#undef X

				throw_undefined_directive(SERVER_KEYWORDS, w);
				/*
				throw error(
					diagnostic::error(w.part(), std::format("unknown directive `{}`", w.str()),
									  std::format("did you mean `{}`?", get_suggestion(SERVER_KEYWORDS, w.str()))));*/
			}

			return config;
		}

		std::vector<server_config> server_config::parse_servers(parse_session& session) {
			std::vector<server_config> configs;

			while (true) {
				session.ignore_ws();

				if (session.eof())
					break;

				session.expect_word_simple("server", "keyword");
				configs.push_back(server_config::parse(session));
			}

			server_config::lint_configs(configs, session);
			return configs;
		}

		void server_config::lint_configs(const std::vector<server_config>& configs, const parse_session& session) {
			ssl_lint(configs);
			server_name_lint(configs);
			(void)session;
		}

		void server_config::ssl_lint(const std::vector<server_config>& configs) {
			std::map<listen_address, file_part> plain_ports;
			std::map<listen_address, file_part> ssl_ports;

			for (auto& config : configs) {
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
							diagnostic diag = diagnostic::error(config._ssl->part,
																"a ssl address cannot be listened without ssl",
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

		void server_config::server_name_lint(const std::vector<server_config>& configs) {
			std::map<listen_address, std::vector<std::reference_wrapper<const server_config>>> servers;

			for (auto& config : configs) {
				for (auto& address : config._addresses) {
					servers[address].push_back(config);
				}
			}

			for (auto& [address, configs] : servers) {
				if (configs.size() < 2)
					continue;
				std::unordered_map<std::string_view, file_part> names;

				for (auto& config : configs) {
					if (config.get()._server_names.empty()) {
                                                diagnostic diag = diagnostic::error(file_part(std::nullopt, buf_pos(0, 0), buf_pos(5, 0)), "test");

                                                throw error(diag);
                                                //TODO diagnostic error ambigious server
					}

					for (auto& [name, part] : config.get()._server_names) {
						auto [it, inserted] = names.insert({name, part});
						
						if (!inserted) {
							diagnostic diag = diagnostic::error(part, "ambigious server", "another config has the same server_name");
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

		block_config block_config::parse(parse_session& session) {
			session.ignore_ws();
			session.expect('{');

			block_config config;

			while (true) {
				session.ignore_ws();

				if (session.peek() == '}') {
					session.get();
					break;
				} else if (session.peek() == '#') {
					config.parse_comment(session);
					continue;
				}

				const word w = session.get_word_simple("keyword");

#define X(name) \
				if (w.str() == #name) {\
					config.parse_##name(session);\
					continue;\
				}
				COBRA_BLOCK_KEYWORDS
#undef X
				throw_undefined_directive(BLOCK_KEYWORDS, w);
			}
			return config;
		}

		void block_config::parse_max_body_size(parse_session& session) {
			const std::size_t define_start = session.column() - std::string("max_body_size").length();
			session.ignore_ws();

			const std::size_t col = session.column();
			const std::size_t line = session.line();

			std::string word;
			try {
				word  = session.get_word_simple("number", "size");
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

				_max_body_size = define<std::size_t>(limit, file_part(session.file(), line, define_start, len));//TODO use parse_define
			} catch (error err) {
				err.diag().message = "invalid max_body_size";
				err.diag().part = file_part(session.file(), line, col, word.length());
				throw err;
			}
		}

		void block_config::parse_location(parse_session& session) {
			auto def = parse_define<location_filter>(session, "location");
			session.ignore_ws();

			define<block_config> block = define<block_config>{block_config::parse(session), std::move(def.part)};
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
					diag.sub_diags.push_back(diagnostic::note(it->second.part, "because of an earlier definition here"));
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
			if (_handler) {//TODO dry
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
			size_t start_col  = session.column();
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

			for (auto [name,_] : cfg._server_names) {
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
	}

}
