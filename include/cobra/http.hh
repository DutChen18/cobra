#ifndef COBRA_HTTP_HH
#define COBRA_HTTP_HH

#include <stdexcept>

#include <memory>
#include "cobra/asio.hh"

namespace cobra {

	enum class http_status_code {
		uri_too_long = 414,
		not_implemented = 501,
	};

	struct http_version {
		unsigned int major = 0;
		unsigned int minor = 0;

		http_version() = default;
		http_version(const http_version& other) = default;
	};

	class http_error : public std::exception {
	public:
		http_error(http_status_code code);
		http_error(const std::string& what);
		http_error(const char* what);
	};

	class header_map {

	};

	enum class http_method  {
		unknown,
		options,
		get,
		head,
		post,
		put,
		del,
		trace,
		connect,
	};

	class http_request {
		http_method method;
		std::string request_uri;
		http_version version;

		header_map _headers;

	public:
		http_request() = default;

		const header_map& headers() const;
		header_map headers();
	};

	http_method parse_method(const std::string& string);
	//future<http_request> parse_request(buffered_istream<& stream);
        future<http_request> parse_request(buffered_isstream& stream);
}

#endif
