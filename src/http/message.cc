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

	// TODO: handle key case
	const http_header_value& http_message::header(const http_header_key& key) const {
		return _header_map.at(key);
	}
	
	// TODO: handle key case
	bool http_message::has_header(const http_header_key& key) const {
		return _header_map.contains(key);
	}

	// TODO: handle key case
	void http_message::set_header(http_header_key key, http_header_value value) {
		_header_map.insert_or_assign(std::move(key), std::move(value));
	}

	http_request::http_request(http_version version, http_request_method method, http_request_uri uri) : http_message(std::move(version)), _method(std::move(method)), _uri(std::move(uri)) {
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
