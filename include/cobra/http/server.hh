#ifndef COBRA_HTTP_SERVER_HH
#define COBRA_HTTP_SERVER_HH

#include "cobra/http/message.hh"
#include "cobra/http/writer.hh"
#include "cobra/net/stream.hh"
#include "cobra/config.hh"

#include <optional>
#include <memory>

namespace cobra {

	class http_filter {
		std::optional<config::filter> _filter;
		config::config _config;
		//config::block_config _config;
		std::vector<http_filter> _sub_filters;
		std::string _path; //TODO use uri

	protected:
		http_filter() = delete;
		http_filter(std::vector<http_filter> sub_filters);
	
		http_filter(const std::string& location, config::config config);
	public:
		http_filter(config::config config);

		http_filter* match(const http_request& request);

		inline std::string_view path() const { return _path; };
		inline const config::config& config() const { return _config; }

	protected:
		bool eval(const http_request& request) const;
	};

	class server : public http_filter {
		config::listen_address _address;

		server() = delete;
		server(config::listen_address address, std::vector<http_filter> handlers);
	public:
		task<void> start(executor* exec, event_loop *loop);

		static std::vector<server> convert(const config::server& config);
		//static std::vector<server> convert(const std::vector<config::server_config>& configs);
	private:
		task<void> on_connect(socket_stream socket);
		static task<void> handle_request(const http_filter& config, const http_request& request, buffered_istream_reference in, buffered_ostream_reference out);
	};
}

#endif
