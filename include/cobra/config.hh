#ifndef COBRA_CONFIG_HH
#define COBRA_CONFIG_HH

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <filesystem>
#include <unordered_map>
#include <iostream>
#include <utility>
#include <vector>
#include <stdexcept>
#include <limits>
#include <format>

#define COBRA_SERVER_KEYWORDS \
	X(listen)					\
	X(server_name)				\
	X(ssl)

namespace cobra {
	namespace fs = std::filesystem;
	using port = unsigned short;

	class config_diagnostic {
		std::string _message;
		std::optional<fs::path> _file;
		std::pair<std::size_t, std::size_t> _column_span;
		std::string _primary_label;
		std::string _secondary_label;
		std::vector<config_diagnostic> _sub_diagnostics;

	public:
		config_diagnostic() = delete;
		constexpr config_diagnostic(std::string message, std::optional<fs::path> file, std::pair<std::size_t, std::size_t> column_span, std::string primary_label = std::string(), std::string secondary_label = std::string());

		inline std::string message() { return _message; }
		inline const std::string& message() const { return _message; }

		inline std::optional<fs::path>& file()  { return _file; }
		inline const std::optional<fs::path>& file() const { return _file; }

		inline std::pair<std::size_t, std::size_t>& columns() { return _column_span; }
		inline const std::pair<std::size_t, std::size_t>& columns() const { return _column_span; }

		inline  std::string primary_label()  { return _primary_label; }
		inline const std::string primary_label() const { return _primary_label; }

		inline  std::string secondary_label()  { return _secondary_label; }
		inline const std::string secondary_label() const { return _secondary_label; }

		inline std::vector<config_diagnostic>& sub_diagnostics() { return _sub_diagnostics; }
		inline const std::vector<config_diagnostic>& sub_diagnostics() const { return _sub_diagnostics; }
	};

	class config_error : public std::exception {
		config_diagnostic _diagnostic;

	public:
		config_error(config_diagnostic diagnostic);

		inline config_diagnostic& diagnostic() { return _diagnostic; }
		inline const config_diagnostic& diagnostic() const { return _diagnostic; }
	};

	class parse_session {
		//std::vector<std::string> _prev_lines;
		std::istream& _stream;
		std::optional<fs::path> _file;
		std::size_t _column_num;
		std::size_t _line_num;

	public:
		parse_session() = delete;
		parse_session(std::istream& stream);

		template <class UnaryPredicate>
		std::size_t ignore_while(UnaryPredicate p);
		std::size_t ignore_whitespace();
		std::string get_word();
		void expect(int ch);
		int peek();
		void consume(std::size_t count);

		template <class UnsignedInt>
		UnsignedInt get_unsigned() {
			UnsignedInt result(static_cast<UnsignedInt>(0));
			const UnsignedInt max_value = std::numeric_limits<UnsignedInt>::max();
			const std::size_t number_start = column();

			bool first_pass = true;

			while (true) {
				int ch = peek();

				if (!std::isdigit(ch))
					break;

				UnsignedInt digit(static_cast<UnsignedInt>(ch - '0'));
				UnsignedInt tmp = result * static_cast<UnsignedInt>(10);

				if (tmp / static_cast<UnsignedInt>(10) != result || max_value - tmp < digit) {
					const std::size_t number_end = number_start + ignore_while(std::isdigit<char>);

					config_diagnostic diagnostic("number too large", _file, std::make_pair(number_start, number_end), std::format("maximum accepted value is {}", max_value));
					throw config_error(std::move(diagnostic));
				}

				result = tmp + digit;

				consume(1);
				first_pass = false;
			}

			if (first_pass)
				throw config_error(make_diagnostic("Expected at least one digit"));
			return result;
		}

		inline std::size_t column() const { return _column_num; }
		inline std::size_t line() const { return _line_num; }

	private:
		config_diagnostic make_diagnostic(std::string message) const;
	};

	struct ssl_settings {
		fs::path _ssl_cert;
		fs::path _ssl_key;
	};

	class server_config {
		std::optional<std::string> _server_name;
		std::optional<ssl_settings> _ssl;
		std::unordered_map<port, bool> _listen;

		server_config();
	public:
		static server_config parse(parse_session& session);

	private:
		void parse_listen(parse_session& session);
	};
}

#endif
