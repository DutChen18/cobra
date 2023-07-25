#ifndef COBRA_HANDLER_HH
#define COBRA_HANDLER_HH

#include "cobra/asyncio/event_loop.hh"
#include "cobra/asyncio/stream.hh"
#include "cobra/http/writer.hh"

namespace cobra {
	class static_config {
		std::string _root;

	public:
		static_config(std::string root);

		const std::string& root() const;
	};

	class cgi_command {
		std::string _cmd;

	public:
		cgi_command(std::string cmd);

		const std::string& cmd() const;
	};

	class cgi_address {
		std::string _node;
		std::string _service;

	public:
		cgi_address(std::string node, std::string service);

		const std::string& node() const;
		const std::string& service() const;
	};

	// TODO: unix sockets?
	// TODO: named pipes?
	class cgi_config {
		std::string _root;
		std::variant<cgi_command, cgi_address> _config;

	public:
		cgi_config(std::string root, cgi_command cmd);
		cgi_config(std::string root, cgi_address addr);

		const std::string& root() const;

		const cgi_command* cmd() const;
		const cgi_address* addr() const;
	};

	class redirect_config {
		http_response_code _code;
		std::string _root;

	public:
		redirect_config(http_response_code code, std::string root);

		http_response_code code() const;
		const std::string& root() const;
	};

	class proxy_config {
		std::string _node;
		std::string _service;

	public:
		proxy_config(std::string node, std::string service);

		const std::string& node() const;
		const std::string& service() const;
	};

	template <class T>
	class handle_context {
		event_loop* _loop;
		executor* _exec;
		std::string _file;
		std::reference_wrapper<const T> _config;
		std::reference_wrapper<const http_request> _request;
		buffered_istream_reference _istream;

	public:
		handle_context(event_loop* loop, executor* exec, std::string file, const T& config, const http_request& request,
					   buffered_istream_reference istream)
			: _loop(loop), _exec(exec), _file(std::move(file)), _config(config), _request(request), _istream(istream) {}

		event_loop* loop() const {
			return _loop;
		}

		executor* exec() const {
			return _exec;
		}

		const std::string& file() const {
			return _file;
		}

		const T& config() const {
			return _config;
		}

		const http_request& request() const {
			return _request;
		}

		buffered_istream_reference istream() const {
			return _istream;
		}
	};

	task<void> handle_static(http_response_writer writer, const handle_context<static_config>& context);
	task<void> handle_cgi(http_response_writer writer, const handle_context<cgi_config>& context);
	task<void> handle_redirect(http_response_writer writer, const handle_context<redirect_config>& context);
}

#endif
