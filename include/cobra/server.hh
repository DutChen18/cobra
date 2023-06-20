#ifndef COBRA_SERVER_HH
#define COBRA_SERVER_HH

#include "cobra/future.hh"
#include "cobra/asio.hh"
#include "cobra/http.hh"
#include "cobra/socket.hh"
#include "cobra/path.hh"
#include <memory>
#include <vector>

namespace cobra {

	class command;

	class request_info : public istream, public ostream {
		std::shared_ptr<istream> _istream;
		std::shared_ptr<ostream> _ostream;
		http_request _request;
		http_response _response;
		bool _response_sent = false;

	public:
		inline const http_request& request() const { return _request; }
		inline const http_response& response() const { return _response; }
		
		future<std::size_t> read(char_type* dst, std::size_t count) override;
		future<std::size_t> write(const char_type* data, std::size_t count) override;
		future<unit> flush() override;

		future<unit> send_response();
		future<unit> end();
		future<unit> end(istream& is);
	};

	class request_handler {
	protected:
		virtual ~request_handler();

	public:
		virtual future<bool> handle(request_info& info, const std::string& path_info) = 0;
	};

	class static_request_handler : public request_handler {
		path _root;

	public:
		static_request_handler(std::string root);

		future<bool> handle(request_info& info, const std::string& path_info) override;
	};

	class cgi_request_handler : public request_handler {
		path _cgi_path;
		path _script_path;

		optional<command> get_command(const request_info& info, const std::string& path_info) const;

	public:
		cgi_request_handler(std::string cgi_path, std::string script_path);

		future<bool> handle(request_info& info, const std::string& path_info) override;
	};

	class request_filter {
	protected:
		virtual ~request_filter();

		virtual bool match(const http_request& request) const = 0;
	};

	class http_block : public request_filter {
		std::vector<std::unique_ptr<request_filter>> _filters;
		std::vector<std::unique_ptr<http_block>> _children;
		optional<request_handler> _default_handler;
	
	public:
		http_block() = default;

		void add_filter(request_filter&& filter);
		void add_child(std::shared_ptr<http_block> child);
		void set_default_handler(request_handler&& handler);

		std::pair<request_handler*, std::size_t> best_match(const http_request& request);

	protected:
		virtual void apply(http_response& response);
		virtual bool match(const http_request& request) const;
	};

	class http_server : public http_block {
	public:

	protected:
		future<unit> on_connect(connected_socket sock);
	};
}

#endif
