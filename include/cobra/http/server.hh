#ifndef COBRA_HTTP_SERVER_HH
#define COBRA_HTTP_SERVER_HH

#include "cobra/http/message.hh"
#include "cobra/http/writer.hh"
#include "cobra/net/stream.hh"
#include "cobra/config.hh"

#include <optional>
#include <memory>

namespace cobra {

	class http_handler {
		std::optional<config::filter> _filter;
		config::block_config _config;
		std::vector<http_handler> _sub_handlers;

	protected:
		http_handler() = delete;
		http_handler(config::block_config config);
	
	public:
		http_handler(config::filter filter, config::block_config config);
		virtual ~http_handler();

		std::optional<std::reference_wrapper<http_handler>> match(const socket_stream& socket, const http_request& request);
		task<void> operator()(http_response_writer writer, const http_request& request, http_istream stream);

	protected:
		virtual bool eval(const socket_stream& socket, const http_request& request) const;
	};

	class server_handler : public http_handler {
		std::string _server_name;

	public:
		server_handler() = delete;
		server_handler(config::server_config config);

	protected:
		bool eval(const socket_stream& socket, const http_request& request) const;
	};

	class server {
		config::listen_address _address;
		std::vector<std::unique_ptr<http_handler>> _handlers;

		server() = delete;
		server(config::listen_address address, std::vector<std::unique_ptr<http_handler>> handlers);
	public:
		task<void> start(executor* exec, event_loop *loop);

		static std::vector<server> convert(const config::server_config& config);
		static std::vector<server> convert(const std::vector<config::server_config>& configs);
	private:
		task<void> on_connect(socket_stream socket);
		std::optional<std::reference_wrapper<http_handler>> match(const socket_stream& socket, const http_request& request);
	};
}

#endif
