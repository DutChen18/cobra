#include "cobra/http.hh"
#include "cobra/future.hh"
#include <exception>
#include <memory>
#include <cctype>

namespace cobra {

	std::string to_error_text(http_error_code error_code) {
		switch (error_code) {
		case http_error_code::bad_request:
			return "Bad Request";
		case http_error_code::unexpected_eof:
			return "Unexpected EOF";
		case http_error_code::bad_status_digit:
			return "Bad Status Digit";
		case http_error_code::reason_phrase_too_long:
			return "Reason Phrase Too Long";
		case http_error_code::missing_method:
			return "Missing Method";
		case http_error_code::method_too_long:
			return "Method Too Long";
		case http_error_code::bad_method:
			return "Bad Method";
		case http_error_code::request_uri_too_long:
			return "Request URI Too Long";
		case http_error_code::bad_request_uri:
			return "Bad Request URI";
		case http_error_code::bad_version:
			return "Bad Version";
		case http_error_code::missing_major_version:
			return "Missing Major Version";
		case http_error_code::missing_minor_version:
			return "Missing Minor Version";
		case http_error_code::header_key_too_long:
			return "Header Key Too Long";
		case http_error_code::bad_header_key:
			return "Bad Header Key";
		case http_error_code::header_value_too_long:
			return "Header Value Too Long";
		case http_error_code::bad_header_value:
			return "Bad Header Value";
		case http_error_code::missing_cr_before_lf:
			return "Missing CR Before LF";
		case http_error_code::missing_lf_after_cr:
			return "Missing LF After CR";
		default:
			return "Unknown error";
		}
	}

	http_error::http_error(http_error_code error_code) : _base(to_error_text(error_code)), _error_code(error_code) {}
	http_error::http_error(http_error_code error_code, const std::string& what) : _base(what), _error_code(error_code) {}
	http_error::http_error(http_error_code error_code, const char* what) : _base(what), _error_code(error_code) {}

}
