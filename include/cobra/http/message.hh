#ifndef COBRA_HTTP_MESSAGE_HH
#define COBRA_HTTP_MESSAGE_HH

#include <string>
#include <unordered_map>

namespace cobra {
	using http_version_type = unsigned short;
	using http_header_key = std::string;
	using http_header_value = std::string;
	using http_header_map = std::unordered_map<http_header_key, http_header_value>;
	using http_request_method = std::string;
	using http_request_uri = std::string;
	using http_response_code = unsigned short;
	using http_response_reason = std::string;

	class http_version {
		http_version_type _major;
		http_version_type _minor;

	public:
		http_version(http_version_type major, http_version_type minor);

		http_version_type major() const;
		http_version_type minor() const;
	};

	class http_message {
		http_version _version;
		http_header_map _header_map;

	public:
		http_message(http_version version);

		const http_version& version() const;
		void set_version(http_version version);
		const http_header_map& header_map() const;
		void set_header_map(http_header_map header_map);
		const http_header_value& header(const http_header_key& key) const;
		bool has_header(const http_header_key& key) const;
		void set_header(http_header_key key, http_header_value value);
	};

	class http_request : public http_message {
		http_request_method _method;
		http_request_uri _uri;

	public:
		http_request(http_version version, http_request_method method, http_request_uri uri);

		const http_request_method& method() const;
		void set_method(http_request_method method);
		const http_request_uri& uri() const;
		void set_uri(http_request_uri uri);
	};

	class http_response : public http_message {
		http_response_code _code;
		http_response_reason _reason;

	public:
		http_response(http_version version, http_response_code code, http_response_reason reason);

		const http_response_code& code() const;
		void set_code(http_response_code code);
		const http_response_reason& reason() const;
		void set_reason(http_response_reason reason);
	};
}

#endif
