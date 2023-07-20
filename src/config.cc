#include "cobra/config.hh"

#include "cobra/exception.hh"
#include "cobra/print.hh"

#include <any>
#include <cctype>
#include <compare>
#include <cstddef>
#include <memory>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <format>
#include <memory>
#include <unordered_set>
#include <utility>
#include <vector>
#include <cassert>

namespace cobra {

	namespace config {
		constexpr buf_pos::buf_pos(std::size_t line, std::size_t col) noexcept : buf_pos(line, col, 1) {}
		constexpr buf_pos::buf_pos(std::size_t line, std::size_t col, std::size_t length) noexcept
			: line(line), col(col), length(length) {}

		file_part::file_part(std::optional<fs::path> file, std::size_t line, std::size_t col)
			: file_part(std::move(file), line, col, 1) {}
		file_part::file_part(std::optional<fs::path> file, std::size_t line, std::size_t col, std::size_t length)
			: buf_pos(line, col, length), file(std::move(file)) {}
		file_part::file_part(fs::path file, std::size_t line, std::size_t col) : file_part(file, line, col, 1) {}
		file_part::file_part(fs::path file, std::size_t line, std::size_t col, std::size_t length)
			: buf_pos(line, col, length), file(std::move(file)) {}
		file_part::file_part(std::size_t line, std::size_t col) : file_part(line, col, 1) {}
		file_part::file_part(std::size_t line, std::size_t col, std::size_t length)
			: buf_pos(line, col, length), file() {}

		diagnostic::diagnostic(level lvl, file_part part, std::string message) : diagnostic(lvl, part, message, "") {}

		diagnostic::diagnostic(level lvl, file_part part, std::string message, std::string primary_label)
			: diagnostic(lvl, part, message, primary_label, "") {}

		diagnostic::diagnostic(level lvl, file_part part, std::string message, std::string primary_label,
							   std::string secondary_label)
			: lvl(lvl), part(std::move(part)), message(std::move(message)), primary_label(std::move(primary_label)),
			  secondary_label(std::move(secondary_label)) {}

		std::ostream& diagnostic::print(std::ostream& out, const std::vector<std::string>& lines) const {
			cobra::println(out, "{}{}{}{}: {}{}", get_control(lvl) | term::set_bold(), lvl, term::reset(), term::set_bold(), message,
						   term::reset());

			std::string file("<source>");
			if (part.file) {
				file = part.file->string();
			}
			cobra::println(out, " {}-->{} {}:{}:{}", term::fg_blue() | term::set_bold(), term::reset(), file,
						   part.line + 1, part.col + 1);

			const std::string line = std::format("{}", part.line + 1);
			cobra::println(out, "{}{} |{} ", term::fg_blue() | term::set_bold(), std::string(line.length(), ' '),
						   term::reset());
			cobra::print(out, "{}{} |{} ", term::fg_blue() | term::set_bold(), line, term::reset());

			for (auto&& ch : lines.at(part.line)) {
				if (ch == '\t') {
					out << ' ';
				} else {
					out << ch;
				}
			}
			out << std::endl;
			cobra::println(out, "{}{} |{} {}{}{}{}{}", term::fg_blue() | term::set_bold(),
						   std::string(line.length(), ' '), term::reset(), std::string(part.col, ' '),
						   get_control(lvl) | term::set_bold(), std::string(part.length, '^'), term::reset(),
						   secondary_label);
			if (!primary_label.empty()) {
				cobra::println(out, "{}{} |{} {}{}|{}", term::fg_blue() | term::set_bold(),
							   std::string(line.length(), ' '), term::reset(), std::string(part.col, ' '),
							   get_control(lvl) | term::set_bold(), term::reset());
				cobra::println(out, "{}{} |{} {}{}{}{}", term::fg_blue() | term::set_bold(),
							   std::string(line.length(), ' '), term::reset(), std::string(part.col, ' '),
							   get_control(lvl) | term::set_bold(), primary_label, term::reset());
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
		
		parse_session::parse_session(std::istream& stream) : _stream(stream), _lines(), _file(), _col_num(0)/*, _line_num(0)*/ {
			_stream.exceptions(std::ios::badbit | std::ios::failbit);
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

			if (!got)
				throw error(make_error(line(), col, "unexpected EOF", std::format("expected `{}`", static_cast<char>(ch))));
			else if (got != ch)
				throw error(make_error(line(), col, std::format("unexpected `{}`", static_cast<char>(*got)), std::format("expected `{}`", static_cast<char>(ch))));
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
			if (line.empty() && _stream.eof())
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
				//_line_num += 1;

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
				err.diag().part.line = line;
				err.diag().part.col = port_start;
				err.diag().part.length = word.length();
				throw err;
			}
		}

		std::strong_ordering filter::operator<=>(const filter& other) const {
			if (type == other.type)
				return match <=> other.match;
			return type <=> other.type;
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
					diag.print(std::cerr, session.lines());
				}

				_max_body_size = define<std::size_t>(limit, file_part(session.file(), line, define_start, len));//TODO use parse_define
			} catch (error err) {
				err.diag().message = "invalid max_body_size";
				err.diag().part.line = line;
				err.diag().part.col = col;
				err.diag().part.length = word.length();
				throw err;
			}
		}

		void block_config::parse_location(parse_session& session) {
			constexpr std::size_t dir_len = std::string("location").length();
			const file_part part(session.file(), session.line(), session.column() - dir_len, dir_len); 
			session.ignore_ws();

			filter filt{filter::type::location, session.get_word()};
			define<block_config> block = define<block_config>{block_config::parse(session), part};

			auto [it, inserted] = _filters.insert({std::move(filt), std::move(block)});
			if (!inserted) {
				diagnostic diag = diagnostic::warn(part, "unreachable filter");
				diag.sub_diags.push_back(diagnostic::note(it->second.part, "because of an earlier definition here"));
				diag.print(std::cerr, session.lines());
			}
		}

		void block_config::parse_root(parse_session& session) {
			define<static_file_config> def = parse_define<static_file_config>(session, "root");
			if (_handler) {
				diagnostic diag = diagnostic::warn(def.part, "redefinition of request handler");
				diag.sub_diags.push_back(diagnostic::note(_handler->part, "previously defined here"));
				diag.print(std::cerr, session.lines());
			}
			_handler = std::move(def);
		}

		void block_config::parse_index(parse_session& session) {
			define<config_path> def = parse_define<config_path>(session, "index");
			if (_index) {
				diagnostic diag = diagnostic::warn(def.part, "redefinition of index");
				diag.sub_diags.push_back(diagnostic::note(_index->part, "previously defined here"));
				diag.print(std::cerr, session.lines());
			}
			_index = std::move(def);
		}

		config block_config::commit(const config& parent, std::optional<filter> filt) && {
			std::optional<std::size_t> max_body_size = _max_body_size;
			std::optional<fs::path> index;
			if (_index)
				index = std::move(_index->def.path);
			std::optional<std::variant<static_file_config, cgi_config>> handler = _handler;
			std::unordered_set<std::string> server_names;

			for (auto&& [name,_] : _server_names){
				server_names.insert(name);
			}

			if (!max_body_size)
				max_body_size = parent.max_body_size;
			if (!index)
				index = parent.index;
			if (!handler)
				handler = parent.handler;
			if (server_names.empty())
				server_names = parent.server_names;

			config cfg = {std::move(filt), max_body_size, std::move(_headers), std::move(index), std::move(handler), std::move(server_names), std::vector<config>()};

			for (auto&& [filter, block_cfg] : _filters) {
				cfg.sub_configs.push_back(std::move(block_cfg.def).commit(cfg, std::move(filter)));
			}
			return cfg;
		}

		config block_config::commit(std::optional<filter> filt) && {
			std::optional<std::size_t> max_body_size = _max_body_size;
			std::optional<fs::path> index;
			if (_index)
				index = std::move(_index->def.path);
			std::optional<std::variant<static_file_config, cgi_config>> handler = _handler;
			std::unordered_set<std::string> server_names;

			for (auto&& [name,_] : _server_names){
				server_names.insert(name);
			}
			config cfg = {std::move(filt), max_body_size, std::move(_headers), std::move(index), std::move(handler), std::move(server_names), std::vector<config>()};

			for (auto&& [filter, block_cfg] : _filters) {
				cfg.sub_configs.push_back(std::move(block_cfg.def).commit(cfg, std::move(filter)));
			}
			return cfg;
		}

		void server_config::parse_listen(parse_session& session) {
			auto def = parse_define<listen_address>(session, "listen");

			auto [it, inserted] = _addresses.insert(def);
			if (!inserted) {
				diagnostic diag = diagnostic::warn(def.part, "duplicate listen");
				diag.sub_diags.push_back(diagnostic::note(it->part, "previously defined here"));
				diag.print(std::cerr, session.lines());
			}
		}

		void block_config::parse_server_name(parse_session& session) {
			auto def = parse_define<server_name>(session, "server_name");

			auto [it, inserted] = _server_names.insert({def.def.name, def.part});
			if (!inserted) {
				diagnostic diag = diagnostic::warn(def.part, "duplicate server_name");
				diag.sub_diags.push_back(diagnostic::note(it->second, "previously defined here"));
				diag.print(std::cerr, session.lines());
			}
		}

		server server_config::commit() && {
			std::vector<listen_address> addresses;
			addresses.reserve(_addresses.size());

			for (auto&& address : _addresses)
				addresses.push_back(std::move(address));

			config cfg = std::move(*this).block_config::commit();
			return {std::move(cfg), std::move(addresses)};
		}

		void server_config::parse_ssl(parse_session& session) { (void) session; }
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
