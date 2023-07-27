#include "cobra/config.hh"

#include "cobra/exception.hh"
#include "cobra/print.hh"

//TODO check which headers aren't needed anymore
#include <any>
#include <cctype>
#include <compare>
#include <cstddef>
#include <functional>
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
			std::size_t max_length = 0;
			for (std::size_t line_num = part.start.line; line_num <= part.end.line; ++line_num) {
				const std::string& line = lines.at(line_num);
				if (line.length() > max_length)
					max_length = line.length();
			}

			/*
			const std::size_t line_length = std::format("{}", part.end.line).length();
			const std::size_t max_col_length = 80;

			for (std::size_t line_num = part.start.line; line_num <= part.end.line; ++line_num) {
				const std::string& line = lines.at(line_num);
				std::size_t start = 0;
				std::size_t end  = line.length();

				if (max_length > max_col_length) {

				}
				if (line.length() < max_length) {

				}
				cobra::println(out, "{}{{}} |{} {}{}|{}", term::fg_blue() | term::set_bold(),
							   line_num, line_length, term::reset(), std::string(col, ' '),
							   get_control(lvl) | term::set_bold(), term::reset());
			}*/

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

		std::string parse_session::get_word() {
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

		std::string parse_session::get_word_quoted() {
			const std::size_t start_col = column();
			const std::size_t start_line = line();
			consume(1);

			std::string res;

			bool escaped = false;
			while (true) {
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
			return res;
		}

		std::string parse_session::get_word_simple() {
			std::string res;

			for (auto ch : remaining()) {
				if (!std::isgraph(ch))
					break;
				res.push_back(ch);
			}

			if (res.empty())
				throw error(make_error("invalid word", "expected at least one graphical character"));
			consume(res.length());
			return res;
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

				if (line)
					_lines.push_back(std::move(*line));
				return fill_buf();
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

		void parse_session::consume(std::size_t count) {
			assert(remaining().length() >= count && "tried to consume more than available");
			_col_num += count;
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

		server_config server_config::parse(parse_session& session) {
			session.ignore_ws();
			session.expect('{');

			server_config config;

			while (true) {
				session.ignore_ws();

				if (session.peek() == '}') {
					session.get();
					break;
				}

				const std::size_t word_start = session.column();
				const std::size_t word_line = session.line();
				const std::string word = session.get_word_simple();

#define X(name) \
				if (word == #name) {\
					config.parse_##name(session);\
					continue;\
				}
				COBRA_SERVER_KEYWORDS
#undef X
				throw error(session.make_error(word_line, word_start, word.length(), std::format("unknown directive `{}`", word)));
			}

			return config;
		}

		std::vector<server_config> server_config::parse_servers(parse_session& session) {
			std::vector<server_config> configs;

			while (true) {
				session.ignore_ws();

				if (session.eof())
					break;

				session.expect_word_simple("server");
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

			std::size_t port_start = session.column();
			std::size_t line = session.line();
			std::string word = session.get_word_simple();

			auto pos = word.find(':');

			std::string node;

			if (pos != std::string::npos) {
				node = word.substr(0, pos);
				port_start += pos;
				pos += 1;
			} else {
				pos = 0;
			}

			try {
				return listen_address(std::move(node), parse_unsigned<port>(word.substr(pos)));
			} catch (error err) {
				err.diag().message = "invalid port";
				err.diag().part = file_part(session.file(), line, port_start, word.length());
				throw err;
			}
		}

		std::strong_ordering filter::operator<=>(const filter& other) const {
			if (type == other.type)
				return match <=> other.match;
			return type <=> other.type;
		}

		ssl_config ssl_config::parse(parse_session& session) {
			fs::path cert(session.get_word());
			session.ignore_ws();
			fs::path key(session.get_word());
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
				}

				const std::size_t word_start = session.column();
				const std::size_t word_line = session.line();
				const std::string word = session.get_word_simple();

#define X(name) \
				if (word == #name) {\
					config.parse_##name(session);\
					continue;\
				}
				COBRA_BLOCK_KEYWORDS
#undef X
				throw error(session.make_error(word_line, word_start, word.length(), std::format("unknown directive `{}`", word)));
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
				word  = session.get_word_simple();
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
			define<config_path> def = parse_define<config_path>(session, "index");
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

		void server_config::parse_ssl(parse_session& session) {
			_ssl = parse_define<ssl_config>(session, "ssl");
		}

		config::config(config* parent, const block_config& cfg)
			: parent(parent), max_body_size(cfg._max_body_size), handler(cfg._handler) {
			if (cfg._index)
				index = cfg._index->def.path;

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

/*
int main() {
	cobra::config::parse_session session(std::cin);

	try {
		cobra::config::server_config::parse(session);
	} catch (const cobra::config::error& err) {
		err.diag().print(std::cerr, session.lines());
	}
}*/
