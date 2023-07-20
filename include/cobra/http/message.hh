#ifndef COBRA_HTTP_MESSAGE_HH
#define COBRA_HTTP_MESSAGE_HH

#include "cobra/http/uri.hh"

#include <string>
#include <unordered_map>

#define HTTP_OK 200
#define HTTP_CREATED 201
#define HTTP_ACCEPTED 202
#define HTTP_NON_AUTHORITATIVE_INFORMATION 203
#define HTTP_NO_CONTENT 204
#define HTTP_RESET_CONTENT 205
#define HTTP_PARTIAL_CONTENT 206
#define HTTP_MULTIPLE_CHOICES 300
#define HTTP_MOVED_PERMANENTLY 301
#define HTTP_FOUND 302
#define HTTP_SEE_OTHER 303
#define HTTP_NOT_MODIFIED 304
#define HTTP_USE_PROXY 305
#define HTTP_TEMPORARY_REDIRECT 307
#define HTTP_PERMANENT_REDIRECT 308
#define HTTP_BAD_REQUEST 400
#define HTTP_UNAUTHORIZED 401
#define HTTP_PAYMENT_REQUIRED 402
#define HTTP_FORBIDDEN 403
#define HTTP_NOT_FOUND 404
#define HTTP_METHOD_NOT_ALLOWED 405
#define HTTP_NOT_ACCEPTABLE 406
#define HTTP_PROXY_AUTHENTICATION_REQUIRED 407
#define HTTP_REQUEST_TIMED_OUT 408
#define HTTP_CONFLICT 409
#define HTTP_GONE 410
#define HTTP_LENGTH_REQUIRED 411
#define HTTP_PRECONDITION_FAILED 412
#define HTTP_CONTENT_TOO_LARGE 413
#define HTTP_URI_TOO_LONG 414
#define HTTP_UNSUPPORTED_MEDIA_TYPE 415
#define HTTP_RANGE_NOT_SATISFIABLE 416
#define HTTP_EXPECTATION_FAILED 417
#define HTTP_MISDIRECTED_REQUEST 421
#define HTTP_UNPROCESSABLE_CONTENT 422
#define HTTP_UPGRADE_REQUIRED 426
#define HTTP_INTERNAL_SERVER_ERROR 500
#define HTTP_NOT_IMPLEMENTED 501
#define HTTP_BAD_GATEWAY 502
#define HTTP_SERVICE_UNAVAILABLE 503
#define HTTP_GATEWAY_TIMEOUT 504
#define HTTP_HTTP_VERSION_NOT_SUPORTED 505

namespace cobra {
	using http_version_type = unsigned short;
	using http_header_key = std::string;
	using http_header_value = std::string;
	using http_request_method = std::string;
	using http_request_uri = uri;
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

	class http_header_map {
		using map_type = std::unordered_map<http_header_key, http_header_value>;

		map_type _map;

	public:
		using iterator = map_type::iterator;
		using const_iterator = map_type::const_iterator;

		const http_header_value& at(const http_header_key& key) const;
		bool contains(const http_header_key& key) const;
		bool insert(http_header_key key, http_header_value value);
		void insert_or_assign(http_header_key key, http_header_value value);

		iterator begin();
		const_iterator begin() const;
		iterator end();
		const_iterator end() const;
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
		http_request(http_request_method method, http_request_uri uri);

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
		http_response(http_response_code code);

		const http_response_code& code() const;
		void set_code(http_response_code code);
		const http_response_reason& reason() const;
		void set_reason(http_response_reason reason);
	};
}

#endif
