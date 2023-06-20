#ifndef COBRA_CONFIG_HH
#define COBRA_CONFIG_HH

#include "cobra/server.hh"
#include "cobra/path.hh"
#include <exception>
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <set>

#define BLOCK_KEYWORDS	\
	X(set_header)

#define SERVER_KEYWORDS	\
	X(listen)			\
	X(server_name)		\
	X(ssl_cert)			\
	X(ssl_key)

namespace cobra {

	enum class server_keyword {
#define X(name) name,
SERVER_KEYWORDS
#undef X
	};

	enum class block_keyword {
#define X(name) name,
BLOCK_KEYWORDS
#undef X
	};

	class config_error : public std::exception {};

	struct block_config {
		std::map<std::string, std::string> headers;
		std::unique_ptr<request_handler> handler;
		std::map<path, block_config> locations;
	};

	struct server_config : public block_config {
		std::string server_name;
		std::map<unsigned short, bool> listen;
		optional<path> ssl_cert;
		optional<path> ssl_key;
	};

	class config {
		std::multimap<unsigned short, server_config> _servers;

	public:
		constexpr static std::size_t max_word_length = 1024;
		constexpr static std::size_t max_path_length = 1024;

		config() = default;

		void add_server(server_config config);
	};
};

#endif
