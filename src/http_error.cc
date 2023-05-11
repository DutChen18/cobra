#include "cobra/http.hh"
#include "cobra/future.hh"
#include <exception>
#include <memory>
#include <cctype>

namespace cobra {

	std::string to_status_text(http_status_code status_code) {
		switch (status_code) {
		case http_status_code::uri_too_long:
			return "Uri too long";
		case http_status_code::not_implemented:
			return "Not implemented";
		case http_status_code::bad_request:
			return "Bad request";
		default:
			return "Unknown error";
		}
	}

	http_error::http_error(http_status_code status_code) : _base(to_status_text(status_code)), _status_code(status_code) {}
	http_error::http_error(http_status_code status_code, const std::string& what) : _base(what), _status_code(status_code) {}
	http_error::http_error(http_status_code status_code, const char* what) : _base(what), _status_code(status_code) {}

}
