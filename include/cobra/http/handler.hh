#ifndef COBRA_HANDLER_HH
#define COBRA_HANDLER_HH

#include "cobra/asyncio/event_loop.hh"
#include "cobra/asyncio/stream.hh"
#include "cobra/http/writer.hh"
#include <optional>

namespace cobra {
	class static_config {
		std::optional<http_response_code> _code;

	public:
		static_config(std::optional<http_response_code> code = std::nullopt) : _code(code) {}

		inline std::optional<http_response_code> code() const { return _code; }
	};

	class cgi_command {
		std::string _cmd;

	public:
		cgi_command(std::string cmd) : _cmd(std::move(cmd)) {}

		inline const std::string& cmd() const {
			return _cmd;
		}
	};

	class cgi_address {
		std::string _node;
		std::string _service;

	public:
		cgi_address(std::string node, std::string service) : _node(std::move(node)), _service(std::move(service)) {}

		inline const std::string& node() const {
			return _node;
		}

		inline const std::string& service() const {
			return _service;
		}
	};

	// TODO: unix sockets?
	// TODO: named pipes?
	class cgi_config {
		std::variant<cgi_command, cgi_address> _config;

	public:
		cgi_config(cgi_command cmd) : _config(std::move(cmd)) {}
		cgi_config(cgi_address addr) : _config(std::move(addr)) {}

		inline const cgi_command* cmd() const {
			return std::get_if<cgi_command>(&_config);
		}

		inline const cgi_address* addr() const {
			return std::get_if<cgi_address>(&_config);
		}
	};

	class redirect_config {
		http_response_code _code;
		std::string _root;

	public:
		redirect_config(http_response_code code, std::string root) : _code(code), _root(root) {}

		inline http_response_code code() const {
			return _code;
		}

		inline const std::string& root() const {
			return _root;
		}
	};

	class proxy_config {
		std::string _node;
		std::string _service;

	public:
		inline proxy_config(std::string node, std::string service) : _node(std::move(node)), _service(std::move(service)) {}

		inline const std::string& node() const {
			return _node;
		}

		inline const std::string& service() const {
			return _service;
		}
	};

	template <class T>
	class handle_context {
		event_loop* _loop;
		executor* _exec;
		std::string _root;
		std::string _file;
		std::reference_wrapper<const T> _config;
		std::reference_wrapper<const http_request> _request;
		buffered_istream_reference _istream;

	public:
		handle_context(event_loop* loop, executor* exec, std::string root, std::string file, const T& config, const http_request& request,
					   buffered_istream_reference istream)
			: _loop(loop), _exec(exec), _root(std::move(root)), _file(std::move(file)), _config(config), _request(request), _istream(istream) {}

		event_loop* loop() const {
			return _loop;
		}

		executor* exec() const {
			return _exec;
		}

		const std::string& root() const {
			return _root;
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
	task<void> handle_proxy(http_response_writer writer, const handle_context<proxy_config>& context);
}

#endif
