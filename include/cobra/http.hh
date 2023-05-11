#ifndef COBRA_HTTP_HH
#define COBRA_HTTP_HH

#include <stdexcept>

#include <memory>
#include <string>
#include <unordered_map>
#include "cobra/asio.hh"

namespace cobra {

	enum class http_status_code {
		bad_request = 400,
		uri_too_long = 414,
		not_implemented = 501,
	};

	class uri {
		std::string _scheme;
		std::string _opaque;
	};

	class path {
		std::vector<std::string> segments;
	};

	class http_version {
	public:
		using version_type = unsigned int;

	private:
		version_type _major = 0;
		version_type _minor = 0;

	public:
		http_version() = default;
		http_version(version_type major, version_type minor);
		http_version(const http_version& other) = default;

		http_version& operator=(const http_version& other) = default;

		inline version_type major() const { return _major; }
		inline version_type minor() const { return _minor; }
	};

	std::string to_status_text(http_status_code status_code);

	class http_error : public std::runtime_error {
		using _base = std::runtime_error;
		http_status_code _status_code;

	public:
		http_error(http_status_code status_code);
		http_error(http_status_code status_code, const std::string& what);
		http_error(http_status_code status_code, const char* what);

		inline http_status_code status_code() const { return _status_code; }
	};

	class header_map {
		using map_type = std::unordered_map<std::string, std::string>;
		map_type map;

	public:
		void insert(std::string key, std::string value);
		const std::string& at(std::string key) const;
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
		http_method _method;
		std::string _request_uri;
		http_version _version;

		header_map _headers;

	public:
		http_request() = default;
		http_request(http_method method, const std::string& request_uri, http_version version, const header_map& map);//TODO use move semantics

		inline const header_map& headers() const { return _headers; }
		inline header_map& headers() { return _headers; };
	};

	http_method parse_method(const std::string& string);
	//future<http_request> parse_request(buffered_istream<& stream);
	future<header_map> parse_headers(istream& stream);
	future<http_request> parse_request(buffered_istream& stream);
}

#endif
