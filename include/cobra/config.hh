#ifndef COBRA_CONFIG_HH
#define COBRA_CONFIG_HH

#include "cobra/server.hh"
#include <exception>
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <set>

#define BLOCK_KEYWORDS	\
	X(set_header)		\
	X(root)				\
	X(cgi)

#define SERVER_KEYWORDS	\
	X(listen)		\
	X(server_name)

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

	class block_config {
		std::map<std::string, std::string> _headers;
		std::unique_ptr<request_handler> _handler;
	};

	struct server_config : public block_config {
		std::string _server_name;
		bool ssl = false;
		std::set<unsigned short> ports;
	};

	class config {
		std::multimap<unsigned short, server_config> _servers;

	public:
		config() = default;

		void add_server(server_config config);
	};
};

#endif
