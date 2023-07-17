#ifndef COBRA_HTTP_SERVER_HH
#define COBRA_HTTP_SERVER_HH

#include "cobra/http/message.hh"
#include "cobra/http/writer.hh"
#include "cobra/net/stream.hh"
#include "cobra/config.hh"

#include <optional>

namespace cobra {

	class request_handler {
	protected:
		std::vector<http_handler> _sub_handlers;

	public:
		virtual ~request_handler();
		virtual void operator()(http_response_writer writer, const http_request& request, http_istream stream) = 0;
		std::optional<request_handler> match(const socket_stream& socket, const http_request& request);

	protected:
		virtual bool eval(const socket_stream& socket, const http_request& request) const = 0;
	};

	class http_handler : public request_handler {
		std::optional<config::filter> _filter;
		config::block_config _config;

	protected:
		http_handler() = delete;
		http_handler(config::block_config config);
	
	public:
		http_handler(config::filter filter, config::block_config config);
		virtual ~http_handler();

		void operator()(http_response_writer writer, const http_request& request, http_istream stream) override;

	protected:
		bool eval(const socket_stream& socket, const http_request& request) const override;
	};

	class server_handler : public http_handler {
		port _port;
		std::string _server_name;

	public:
		server_handler() = delete;
		server_handler(port port, config::server_config config);

	protected:
		bool eval(const socket_stream& socket, const http_request& request) const override;
	};

	class server {
		std::vector<request_handler> _handlers;

		server() = delete;
		server(port p, config::server_config config);
	public:
		void start();

	private:
		task<void> on_connect(socket_stream socket);
		std::optional<request_handler> match(const socket_stream& socket, const http_request& request);
		task<http_request> parse_request(buffered_istream_reference stream);
	};
}

#endif
