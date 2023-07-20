#include "cobra/http/message.hh"

namespace cobra {
	http_version::http_version(http_version_type major, http_version_type minor) : _major(major), _minor(minor) {
	}

	http_version_type http_version::major() const {
		return _major;
	}

	http_version_type http_version::minor() const {
		return _minor;
	}

	static http_header_key key_case(http_header_key key) {
		bool special = true;

		for (char& ch : key) {
			if (special) {
				ch = std::toupper(ch);
			} else {
				ch = std::tolower(ch);
			}

			special = !std::isalpha(ch);
		}

		return key;
	}

	const http_header_value& http_header_map::at(const http_header_key& key) const {
		return _map.at(key_case(key));
	}

	bool http_header_map::contains(const http_header_key& key) const {
		return _map.contains(key_case(key));
	}

	bool http_header_map::insert(http_header_key key, http_header_value value) {
		key = key_case(std::move(key));

		if (_map.contains(key)) {
			return false;
		} else {
			_map.emplace(std::move(key), std::move(value));
			return true;
		}
	}

	void http_header_map::insert_or_assign(http_header_key key, http_header_value value) {
		_map.insert_or_assign(key_case(std::move(key)), std::move(value));
	}

	http_header_map::iterator http_header_map::begin() {
		return _map.begin();
	}

	http_header_map::const_iterator http_header_map::begin() const {
		return _map.begin();
	}

	http_header_map::iterator http_header_map::end() {
		return _map.end();
	}

	http_header_map::const_iterator http_header_map::end() const {
		return _map.end();
	}

	http_message::http_message(http_version version) : _version(std::move(version)) {
	}

	const http_version& http_message::version() const {
		return _version;
	}

	void http_message::set_version(http_version version) {
		_version = std::move(version);
	}

	const http_header_map& http_message::header_map() const {
		return _header_map;
	}

	void http_message::set_header_map(http_header_map header_map) {
		_header_map = std::move(header_map);
	}

	const http_header_value& http_message::header(const http_header_key& key) const {
		return _header_map.at(key);
	}
	
	bool http_message::has_header(const http_header_key& key) const {
		return _header_map.contains(key);
	}

	void http_message::set_header(http_header_key key, http_header_value value) {
		_header_map.insert_or_assign(std::move(key), std::move(value));
	}

	http_request::http_request(http_version version, http_request_method method, http_request_uri uri) : http_message(std::move(version)), _method(std::move(method)), _uri(std::move(uri)) {
	}

	http_request::http_request(http_request_method method, http_request_uri uri) : http_request({ 1, 1 }, std::move(method), std::move(uri)) {
	}

	const http_request_method& http_request::method() const {
		return _method;
	}

	void http_request::set_method(http_request_method method) {
		_method = std::move(method);
	}

	const http_request_uri& http_request::uri() const {
		return _uri;
	}

	void http_request::set_uri(http_request_uri uri) {
		_uri = std::move(uri);
	}

	http_response::http_response(http_version version, http_response_code code, http_response_reason reason) : http_message(std::move(version)), _code(std::move(code)), _reason(std::move(reason)) {
	}

	static http_response_reason get_response_reason(http_response_code code) {
		switch (code) {
		case 200:
			return "OK";
		case 201:
			return "Created";
		case 202:
			return "Accepted";
		case 203:
			return "Non-Authoritative Information";
		case 204:
			return "No Content";
		case 205:
			return "Reset Content";
		case 206:
			return "Partial Content";
		case 300:
			return "Multiple Choices";
		case 301:
			return "Moved Permanently";
		case 302:
			return "Found";
		case 303:
			return "See Other";
		case 304:
			return "Not Modified";
		case 305:
			return "Use Proxy";
		case 307:
			return "Temporary Redirect";
		case 308:
			return "Permanent Redirect";
		case 400:
			return "Bad Request";
		case 401:
			return "Unauthorized";
		case 402:
			return "Payment Required";
		case 403:
			return "Forbidden";
		case 404:
			return "Not Found";
		case 405:
			return "Method Not Allowed";
		case 406:
			return "Not Acceptable";
		case 407:
			return "Proxy Authentication Required";
		case 408:
			return "Request Timed Out";
		case 409:
			return "Conflict";
		case 410:
			return "Gone";
		case 411:
			return "Length Required";
		case 412:
			return "Precondition Failed";
		case 413:
			return "Content Too Large";
		case 414:
			return "URI Too Long";
		case 415:
			return "Unsupported Media Type";
		case 416:
			return "Range Not Satisfiable";
		case 417:
			return "Expectation Failed";
		case 421:
			return "Misdirected Request";
		case 422:
			return "Unprocessable Content";
		case 426:
			return "Upgrade Required";
		case 500:
			return "Internal Server Error";
		case 501:
			return "Not Implemented";
		case 502:
			return "Bad Gateway";
		case 503:
			return "Service Unavailable";
		case 504:
			return "Gateway Timeout";
		case 505:
			return "HTTP Version Not Suported";
		default:
			return "?";
		}
	}

	http_response::http_response(http_response_code code) : http_response({ 1, 1 }, code, get_response_reason(code)) {
	}

	const http_response_code& http_response::code() const {
		return _code;
	}

	void http_response::set_code(http_response_code code) {
		_code = std::move(code);
	}

	const http_response_reason& http_response::reason() const {
		return _reason;
	}

	void http_response::set_reason(http_response_reason reason) {
		_reason = std::move(reason);
	}
}
