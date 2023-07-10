#ifndef COBRA_CONFIG_HH
#define COBRA_CONFIG_HH

#include <algorithm>
#include <any>
#include <cctype>
#include <istream>
#include <optional>
#include <string>
#include <filesystem>
#include <map>
#include <iostream>
#include <utility>
#include <vector>
#include <stdexcept>
#include <limits>
#include <format>

#define COBRA_SERVER_KEYWORDS \
	X(listen) \
	X(ssl)

namespace cobra {
	namespace fs = std::filesystem;
	using port = unsigned short;

	struct config_diagnostic {
		std::string message;
		std::optional<fs::path> file;
		std::size_t line;
		std::pair<std::size_t, std::size_t> column_span;
		std::string primary_label;
		std::string secondary_label;
		std::vector<config_diagnostic> sub_diagnostics;
	};


	class config_error : public std::exception {
		config_diagnostic _diagnostic;

	public:
		config_error(config_diagnostic diagnostic);

		inline config_diagnostic& diagnostic() { return _diagnostic; }
		inline const config_diagnostic& diagnostic() const { return _diagnostic; }
	};

	template <class UnsignedInt>
	UnsignedInt parse_unsigned(const std::string &str) {
		UnsignedInt result(static_cast<UnsignedInt>(0));
		const UnsignedInt max_value = std::numeric_limits<UnsignedInt>::max();

		bool first_pass = true;

		for (auto&& ch : str) {
			if (!std::isdigit(ch))
				break;

			UnsignedInt digit(static_cast<UnsignedInt>(ch - '0'));
			UnsignedInt tmp = result * static_cast<UnsignedInt>(10);

			if (tmp / static_cast<UnsignedInt>(10) != result || max_value - tmp < digit) {
				config_diagnostic diagnostic {"number too large", std::nullopt, 0, std::make_pair(0, str.length()), std::format("maximum accepted value is {}", max_value), std::string()};
				throw config_error(std::move(diagnostic));
			}

			result = tmp + digit;
			first_pass = false;
		}

		if (first_pass) {
			config_diagnostic diagnostic {"expected at least one digit", std::nullopt, 0, std::make_pair(0, 1), std::string(), std::string(), std::vector<config_diagnostic>()};
			throw config_error(std::move(diagnostic));
		}
		return result;
	}

	//TODO add eof, badbit, etc... checks
	class parse_session {
		std::vector<std::string> _lines;
		std::istream& _stream;
		std::optional<fs::path> _file;
		std::size_t _column_num;
		std::size_t _line_num;

	public:
		parse_session() = delete;
		parse_session(std::istream& stream);

		std::size_t ignore_whitespace();
		std::string get_word();
		void expect(int ch);
		int peek();
		int get();
		void consume(std::size_t count);

		template<class UnaryPredicate>
		std::string take_while(UnaryPredicate p) {
			std::string result;

			while (true) {
				const int ch = peek();
				if (_stream.eof() || _stream.bad() || _stream.fail())
					break;
				if (p(ch)) {
					result.push_back(ch);
					consume(1);
				} else {
					break;
				}
			}
			return result;
		}

		template <class UnsignedInt>
		UnsignedInt get_unsigned() {
			const std::size_t start = column();
			std::string digit_str = take_while(std::isdigit<char>);

			try {
				return parse_unsigned<UnsignedInt>(digit_str);
			} catch (const config_error& error) {
				const config_diagnostic& parse_diag = error.diagnostic();
				config_diagnostic diag = make_diagnostic(parse_diag.message, parse_diag.primary_label, start + parse_diag.column_span.first, start + parse_diag.column_span.second);
				diag.secondary_label = parse_diag.secondary_label;

				throw config_error(std::move(diag));
			}
		}

		fs::path get_path();

		inline std::size_t column() const { return _column_num; }
		inline std::size_t line() const { return _line_num; }
		inline std::optional<fs::path> file() const { return _file; }

		inline config_diagnostic make_diagnostic(std::string message) const {
			return make_diagnostic(std::move(message), column(), column());
		}

		inline config_diagnostic make_diagnostic(std::string message, std::size_t column_start, std::size_t columns) const {
			return make_diagnostic(std::move(message), std::string(), column_start, columns);
		}

		inline config_diagnostic make_diagnostic(std::string message, std::string primary_label, std::string secondary_label, std::size_t column_start, std::size_t columns) const {
			return config_diagnostic { std::move(message), file(), line(), std::make_pair(column_start, columns), std::move(primary_label), std::move(secondary_label), std::vector<config_diagnostic>() };
		}

		inline config_diagnostic make_diagnostic(std::string message, std::string primary_label, std::size_t column_start, std::size_t columns) const {
			return config_diagnostic { std::move(message), file(), line(), std::make_pair(column_start, columns), std::move(primary_label), std::string(), std::vector<config_diagnostic>() };
		}

		inline config_diagnostic make_diagnostic(std::string message, std::string primary_label, std::size_t column) const { return make_diagnostic(std::move(message), std::move(primary_label), column, 1); }

		void print_diagnostic(const config_diagnostic& diagnostic, std::ostream& out) const;
		void print_subdiagnostic(const config_diagnostic& diagnostic, std::ostream& out) const;
	};

	class listen_address {
		std::optional<std::string> _node;
		port _service;
	
		listen_address() = default;
	public:
		listen_address(std::string node, port service);
		listen_address(port service);

		static listen_address parse(parse_session& session);
	};

	struct ssl_settings {
		fs::path _ssl_cert;
		fs::path _ssl_key;
	};

	class server_config {
		std::optional<std::string> _server_name;
		std::optional<ssl_settings> _ssl;
		std::vector<listen_address> _listen;

		server_config() = default;
	public:
		static server_config parse(parse_session& session);

	private:
		void parse_listen(parse_session& session);
		void parse_ssl(parse_session& session);
		void parse_server_name(parse_session& session);
	};
}

#endif
