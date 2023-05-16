#include "cobra/server.hh"
#include "cobra/config.hh"
#include "cobra/asio_utils.hh"
#include <cctype>
#include <memory>

namespace cobra {

	future<std::string> read_word(istream& stream, std::size_t max_length) {
	}

	future<unit> parse_x(block_config &config, istream& stream, block_keyword keyword) {
	}

	template <typename UnsignedT>
	static UnsignedT parse_unsigned(const std::string& str) {
		UnsignedT result(0);
		for (auto&& ch : str) {
			if (!std::isdigit(ch))
				throw config_error();//TODO
			
			result = result * 10 + ch - '0';
		}
		return result;
	}

	future<unit> parse_listen(server_config& config, buffered_istream& stream) {
		return ignore_while(stream, std::isspace<char>).and_then<unit>([&config, &stream]() {
			return async_while<unit>([&config, &stream]() {
				return read_word(stream, 1024).and_then<unit>([&config](std::string word) {
					if (word == "ssl") {
						config.ssl = true;
						return resolve(none<unit>());
					} 

					if (!config.ports.insert(parse_unsigned<unsigned short>(word)).second)
						throw config_error();//TODO
					return resolve(some<unit>());
				});
			});
		});
	}

	static future<unit> assert_string(istream& stream, std::string str) {
		return expect_string(stream, std::move(str)).and_then<unit>([](bool b) {
			if (!b)
				throw config_error();//TODO expected str
			return resolve(some<unit>());
		});
	}

	static future<std::string> get_word(istream& stream, std::size_t max_length) {
		return async_while<std::string>(capture([&stream, max_length](std::string& str) {
			return stream.get().and_then<optional<std::string>>([&str, max_length](optional<int> ch) {
				if (!ch || std::isspace(*ch) || str.size() >= max_length)
					return resolve(some<std::string>(std::move(str)));
				str.push_back(*ch);
				return resolve(none<std::string>());
			});
		}, std::string()));
	}

	future<server_config> parse_server(buffered_istream& stream) {
		return assert_string(stream, "server").and_then<server_config>([&stream](unit) {
			return ignore_while(stream, std::isspace<char>).and_then<server_config>([&stream](std::size_t) {
				return assert_string(stream, "{").and_then<server_config>([&stream](unit) {
					return async_while<server_config>(capture([&stream](server_config& config) {
						return ignore_while(stream, std::isspace<char>).and_then<optional<server_config>>([&stream, &config](std::size_t) {
							return stream.peek().and_then<server_config>([&stream, &config](optional<int> ch) {
								if (!ch)
									throw config_error();//TODO unexpected EOF
								if (ch == '}') {
									stream.get_now();
									return resolve(some<server_config>(std::move(config)));
								}

								return get_word(stream, 1024).and_then<optional<server_config>>([&stream, &config](std::string str) {
									if (str == "listen") {
										return parse_listen(config, stream).and_then<optional<server_config>>([](unit) {
											return resolve(none<server_config>());
										});
									}

									throw config_error();//TODO unknown instruction 
								});
							});
						});
					}, server_config()));
				});
			});
		});
	}

	future<http_server> parse_config(std::shared_ptr<istream> stream) {
		return async_while<http_server>([stream]() {

		});
	}
}
