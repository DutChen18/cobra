#include "cobra/config.hh"

#include "cobra/exception.hh"

#include <cctype>
#include <memory>
#include <type_traits>
#include <format>
#include <memory>

namespace cobra {

	parse_session::parse_session(std::istream& stream) : _stream(stream), _column_num(0), _line_num(0) {

	}

	std::size_t parse_session::ignore_whitespace() {
		std::size_t nignored = 0;

		while (true) {
			int ch = _stream.peek();

			if (!std::isspace(ch))
				break;

			if (ch == '\n') {
				_column_num = 0;
				_line_num += 1;
			} else {
				_column_num += 1;
			}

			_stream.get();
			nignored += 1;
		}
		return nignored;
	}

	std::string parse_session::get_word() {
		std::string word;

		while (true) {
			int ch = _stream.peek();

			if (!std::isprint(ch)) {
				break;
			}

			word.push_back(ch);
			_column_num += 1;
		}

		if (word.empty())
			throw_error("Expected at least one printable character");
		return word;
	}

	server_config server_config::parse(parse_session& session) {
		session.ignore_whitespace();
		session.expect('{');

		server_config config;

		while (true) {
			session.ignore_whitespace();

			if (session.peek() == '}') {
				session.consume();
				break;
			}

			std::string word = session.get_word();
		}
	}
}
