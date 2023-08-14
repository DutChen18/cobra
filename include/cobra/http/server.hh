#ifndef COBRA_HTTP_SERVER_HH
#define COBRA_HTTP_SERVER_HH

#include "cobra/asyncio/executor.hh"
#include "cobra/asyncio/event_loop.hh"
#include "cobra/http/message.hh"
#include "cobra/http/writer.hh"
#include "cobra/net/stream.hh"
#include "cobra/config.hh"

#include <optional>
#include <memory>

namespace cobra {

	class http_filter {
		std::shared_ptr<const config::config> _config;
		std::vector<http_filter> _sub_filters;
		std::size_t _match_count;

		http_filter(std::shared_ptr<const config::config> config, std::size_t match_count);
	protected:
		http_filter() = delete;
	
	public:
		http_filter(std::shared_ptr<const config::config> config);
		http_filter(std::shared_ptr<const config::config> config, std::vector<http_filter> filters);

		generator<std::pair<http_filter*, uri_abs_path>> match(const basic_socket_stream& socket, const http_request& request, const uri_abs_path& normalized);

		inline const config::config& config() const { return *_config.get(); }
		inline std::size_t match_count() const { return _match_count; }

	protected:
		bool eval(const basic_socket_stream& socket, const http_request& request, const uri_abs_path& normalized) const;
	};

	class server : public http_filter {
		config::listen_address _address;
		std::unordered_map<std::string, ssl_ctx> _contexts;
		executor* _exec;
		event_loop* _loop;

		server() = delete;
		server(config::listen_address address, std::unordered_map<std::string, ssl_ctx> contexts,
			   std::vector<http_filter> handlers, executor* exec, event_loop* loop);

	public:
		task<void> start(executor* exec, event_loop *loop);

		static std::vector<server> convert(const std::vector<std::shared_ptr<config::server>>& configs,
										   executor* exec, event_loop* loop);

	private:
		task<void> on_connect(basic_socket_stream& socket);
		task<void> handle_request(const http_filter& config, const http_request& request, const uri_abs_path& normalized, buffered_istream_reference in, http_response_writer writer);
	};
}

#endif
