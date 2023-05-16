#ifndef COBRA_SERVER_HH
#define COBRA_SERVER_HH

#include "cobra/future.hh"
#include "cobra/asio.hh"
#include "cobra/http.hh"
#include "cobra/socket.hh"
#include <memory>
#include <vector>

namespace cobra {

	class request_info {
		std::shared_ptr<istream> _istream;
		std::shared_ptr<ostream> _ostream;
		http_request _request;
		std::string _path_info;

	public:
		inline istream& istream() const { return *_istream; }
		inline ostream& ostream() const { return *_ostream; }
		inline const http_request& request() const { return _request; }
		inline const std::string& path_info() const { return _path_info; }
	};

	class request_handler {
	protected:
		virtual ~request_handler();

	public:
		virtual future<bool> handle(const request_info& info) = 0;
	};

	class static_request_handler : public request_handler {
		std::string _root;

	public:
		static_request_handler(std::string root);

		future<bool> handle(const request_info& info) override;
	};

	class cgi_request_handler : public request_handler {
		std::string _cgi_path;
		std::string _script_path;

	public:
		cgi_request_handler(std::string cgi_path, std::string script_path);

		future<bool> handle(const request_info& info) override;
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
