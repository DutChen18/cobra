#include "cobra/config.hh"

#include "cobra/exception.hh"

#include <cctype>
#include <memory>
#include <optional>
#include <type_traits>
#include <format>
#include <memory>
#include <utility>

namespace cobra {

	config_error::config_error(config_diagnostic diagnostic) : _diagnostic(std::move(diagnostic)) {}

	parse_session::parse_session(std::istream& stream) : _stream(stream), _column_num(0), _line_num(0) {

	}

	std::size_t parse_session::ignore_whitespace() {
		std::size_t nignored = 0;

		while (true) {
			int ch = peek();

			if (!std::isspace(ch))
				break;

			consume(1);
			nignored += 1;
		}
		return nignored;
	}

	std::string parse_session::get_word() {
		std::string word;

		while (true) {
			int ch = peek();

			if (!std::isprint(ch) || std::isspace(ch)) {
				break;
			}

			word.push_back(ch);
			consume(1);
		}

		if (word.empty())
			throw config_error(make_diagnostic("expected at least one printable character (isprint(3))"));
		return word;
	}

	void parse_session::expect(int ch) {
		const std::size_t pos = column();
		const int got = get();

		if (ch != got)
			throw config_error(make_diagnostic("unexpected character", std::format("expected \"{}\"", ch), pos));
	}

	int parse_session::peek() {
		return _stream.peek();
	}

	int parse_session::get() {
		int ch = peek();
		consume(1);
		return ch;
	}

	void parse_session::consume(std::size_t count) {
		while (count--) {
			const int ch = _stream.get();

			if (ch == '\n') {
				_column_num = 0;
				_line_num += 1;
			} else {
				while (_line_num >= _lines.size())
					_lines.push_back(std::string());
				if (ch == '\t') {
					_lines[_line_num].push_back(' ');
				} else {
					_lines[_line_num].push_back(ch);
				}

				_column_num += 1;
			}
		}
	}

	fs::path parse_session::get_path() {
		std::string part;

		if (peek() == '"') {
			const std::size_t start_column = column();
			const std::size_t start_line = line();
			consume(1);
			part = take_while([](int ch) { return ch != '"'; });

			if (peek() != '"')
				throw config_error(config_diagnostic { "unclosed \"", file(), start_line, std::make_pair(start_column, 1), "", "", std::vector<config_diagnostic>() });

			consume(1);
		} else {
			part = take_while([](int ch) { return !std::isspace(ch); });
		}

		return fs::path(std::move(part));
	}

	void parse_session::print_diagnostic(const config_diagnostic& diagnostic, std::ostream& out) const {
		out << "error: " << diagnostic.message << std::endl;
		out << "  --> " << diagnostic.file.value_or("<source>") << ":" << diagnostic.line << ":" << diagnostic.column_span.first;

		std::string line = std::format("{}", diagnostic.line + 1);

		out << std::endl;
		out << std::string(line.length(), ' ') << " | " << std::endl;
		out << line << " | " << _lines[diagnostic.line] << std::endl;

		out << std::string(line.length(), ' ') << " | " << std::string(diagnostic.column_span.first, ' ');
		out << std::string(diagnostic.column_span.second, '^') << " " << diagnostic.secondary_label;
		if (!diagnostic.primary_label.empty()) {
			out << std::endl;
			out << std::string(line.length(), ' ') << " | " << std::string(diagnostic.column_span.first, ' ') << '|' << std::endl;
			out << std::string(line.length(), ' ') << " | " << std::string(diagnostic.column_span.first, ' ') << diagnostic.primary_label;
		}

		/*
		for (const config_diagnostic& sub_diagnostic : diagnostic.sub_diagnostics) {
			out << std::endl;
			print_subdiagnostic(sub_diagnostic, out);
		}*/

		out << std::endl;
	}

	listen_address::listen_address(std::string node, port service) : _node(std::move(node)), _service(service) {}
	listen_address::listen_address(port service) : _node(std::nullopt), _service(service) {}

	listen_address listen_address::parse(parse_session& session) {
		const std::size_t word_start = session.column();
		std::string word = session.get_word();

		auto pos = word.find(':');
		listen_address address;

		if (pos != std::string::npos) {
			address._node = word.substr(0, pos);
			word = word.substr(pos);
		}

		try {
			return listen_address(parse_unsigned<port>(word));
		} catch (const config_error& error) {
			const config_diagnostic& sub_diag = error.diagnostic();

			throw config_error(session.make_diagnostic("invalid port", sub_diag.primary_label, sub_diag.message, word_start + sub_diag.column_span.first, sub_diag.column_span.second));
		}
		return address;
	}

	server_config server_config::parse(parse_session& session) {
		session.ignore_whitespace();
		session.expect('{');

		server_config config;

		while (true) {
			session.ignore_whitespace();

			if (session.peek() == '}') {
				session.consume(1);
				break;
			}

			const std::size_t word_start = session.column();
			const std::string word = session.get_word();

#define X(name) \
			if (word == #name) {\
				config.parse_##name(session); \
				continue; \
			}
			COBRA_SERVER_KEYWORDS
#undef X

			throw config_error(session.make_diagnostic(std::format("unknown directive '{}'", word), word_start, word.length()));
		}
		return config;
	}

	void server_config::parse_listen(parse_session& session) {
		session.ignore_whitespace();

		listen_address address = listen_address::parse(session);
		_listen.push_back(address);
	}

	void server_config::parse_ssl(parse_session& session) {
		session.ignore_whitespace();

		fs::path cert = session.get_path();
		session.ignore_whitespace();
		fs::path key = session.get_path();

		_ssl = ssl_settings { cert, key };
	}

	void server_config::parse_server_name(parse_session& session) {
		if (_server_name) {
			//todo give warning about redefinition
		}

		_server_name = session.get_word();
	}
}

int main() {
	cobra::parse_session session(std::cin);

	try {
		cobra::server_config::parse(session);
	} catch (const cobra::config_error& error) {
		session.print_diagnostic(error.diagnostic(), std::cerr);
	}
}
