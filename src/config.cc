#include "cobra/server.hh"
#include "cobra/config.hh"
#include "cobra/asio_utils.hh"
#include "cobra/http.hh"
#include <cctype>
#include <memory>

namespace cobra {

	future<std::string> read_word(istream& stream, std::size_t max_length) {
	}

	future<unit> parse_x(block_config &config, istream& stream, block_keyword keyword) {
	}

	static future<std::string> get_token(buffered_istream& stream) {
		return get_while(stream, static_cast<bool(*)(int)>(is_token)).and_then<std::string>([](std::string token) {
			if (token.empty())
				throw config_error(); //TODO not a token
			return resolve(token);
		});
	}

	static future<std::size_t> ignore_whitespace(buffered_istream &stream) {
		return ignore_while(stream, std::isspace<char>).and_then<std::size_t>([](std::size_t nignored) {
			return resolve(nignored);
		});
	}

	static future<std::string> read_quoted(buffered_istream &stream) {
		return stream.peek().and_then<std::string>([&stream](optional<int> ch) {
			if (!ch || ch != '"')
				throw config_error(); //TODO not a quoted string
			stream.get_now();
			std::size_t quotes = 0;
			return async_while<std::string>([&stream, quotes]() mutable {
			});
		});
		/*
		return async_while<std::string>([&stream, quotes]() mutable {
		});
	}

	static future<unit> parse_set_header(block_config& config, buffered_istream &stream) {
		return get_token(stream).and_then<unit>([&config, &stream](std::string token) {
			return ignore_whitespace(stream).and_then<unit>([&config, &stream](std::size_t) {
				std::size_t brackets = 0;
				return async_while<unit>([&config, &stream, brackers]() mutable {
					
				});
			});
		});
	}

	static future<std::string> get_word(buffered_istream& stream, std::size_t max_length) {
		return ignore_while(stream, std::isspace<char>).and_then<std::string>([&stream, max_length](std::size_t) {
			return async_while<std::string>(capture([&stream, max_length](std::string& str) {
				return stream.get().and_then<optional<std::string>>([&str, max_length](optional<int> ch) {//TODO ability to escape special chars
					if (!ch || std::isspace(*ch) || str.size() >= max_length)
						return resolve(some<std::string>(std::move(str)));
					str.push_back(*ch);
					return resolve(none<std::string>());
				});
			}, std::string()));
		});
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
			bool ssl = false;
			return async_while<unit>([&config, &stream, ssl]() mutable {
				return read_word(stream, config::max_word_length).and_then<unit>([&config, &ssl](std::string word) {
					if (word == "ssl") {
						ssl = true;
						return resolve(none<unit>());
					} 

					if (!config.listen.insert(std::make_pair(parse_unsigned<unsigned short>(word), ssl)).second)
						throw config_error();//TODO error redefinition
					return resolve(some<unit>());
				});
			});
		});
	}

	future<unit> parse_server_name(server_config& config, buffered_istream& stream) {
		return ignore_while(stream, std::isspace<char>).and_then<unit>([&config, &stream]() {
			return get_word(stream, config::max_word_length).and_then<unit>([&config](std::string word) {
				config.server_name = word;
				return resolve(some<unit>());
			});
		});
	}

	future<path> parse_path(buffered_istream& stream) {
		return get_word(stream, config::max_path_length).and_then<path>([](std::string word) {
			return resolve(path(word));
		});
	}

	future<unit> parse_ssl_cert(server_config& config, buffered_istream& stream) {
		return parse_path(stream).and_then<unit>([&config](path cert) {
			if (config.ssl_cert)
				throw config_error(); //TODO multiple certificates set
			config.ssl_cert = some<path>(std::move(cert));
		});
	}

	future<unit> parse_ssl_key(server_config& config, buffered_istream& stream) {
		return parse_path(stream).and_then<unit>([&config](path key) {
			if (config.ssl_key)
				throw config_error(); //TODO multiple keys set
			config.ssl_cert = some<path>(std::move(key));
		});
	}

	static future<unit> assert_string(istream& stream, std::string str) {
		return expect_string(stream, std::move(str)).and_then<unit>([](bool b) {
			if (!b)
				throw config_error();//TODO expected str
			return resolve(some<unit>());
		});
	}

	future<unit> parse_location(server_config& config, buffered_istream& stream) {
		return parse_path(stream).and_then<unit>([&config, &stream](path prefix) {
			if (config.locations.count(prefix) != 0) {
				throw config_error(); //TODO ambigious location
			}

			//TODO move prefix instead of references
			return ignore_while(stream, std::isspace<char>).and_then<unit>([&config, &stream, &prefix](std::size_t) {
				return assert_string(stream, "{").and_then<unit>([&config, &stream, &prefix](unit) {
					return async_while<unit>(capture([&config, &stream, &prefix](block_config& block) {
						return ignore_while(stream, std::isspace<char>).and_then<optional<unit>>([&config, &stream, &block, &prefix](std::size_t) {
							return stream.peek().and_then<optional<unit>>([&stream, &config, &block, &prefix](optional<int> ch) {
								if (!ch) {
									throw config_error(); //TODO unexpected EOF
								}
								if (ch == '}') {
									stream.get_now();
									return resolve(some<unit>());
								}

								return get_word(stream, config::max_word_length).and_then<optional<unit>>([&stream, &block](std::string word) {
#define X(name) \
									if (word == #name) { \
										return parse_##name(block, stream).and_then<optional<unit>>([](unit) { \
											return resolve(none<unit>()); \
										}); \
									}
									BLOCK_KEYWORDS
#undef X
								});
							});
						});
					}, block_config()));
				});
			});
		});
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


								return get_word(stream, config::max_word_length).and_then<optional<server_config>>([&stream, &config](std::string str) {
#define X(name) \
									if (str == #name) { \
										return parse_##name(config, stream).and_then<optional<server_config>>([](unit) { \
											return resolve(none<server_config>()); 		\
										}); \
									}
									SERVER_KEYWORDS
#undef X
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
