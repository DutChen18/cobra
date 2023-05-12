#include "cobra/http.hh"

namespace cobra {
	http_message::http_message(http_version version) : _version(version) {
	}

	http_message::http_message(http_version version, header_map map) : _headers(std::move(map)), _version(version) {
	}

	http_request::http_request(std::string method, std::string request_uri, http_version version) : http_message(version), _method(method), _request_uri(std::move(request_uri)) {
	}

	http_request::http_request(std::string method, std::string request_uri, http_version version, header_map map) : http_message(version, std::move(map)), _method(method), _request_uri(std::move(request_uri)) {
	}

	http_response::http_response(http_version version, unsigned int status_code, std::string reason_phrase) : http_message(version), _status_code(status_code), _reason_phrase(reason_phrase) {
	}

	http_response::http_response(http_version version, unsigned int status_code, std::string reason_phrase, header_map map) : http_message(version, std::move(map)), _status_code(status_code), _reason_phrase(reason_phrase) {
	}

	http_response::http_response(http_version version, http_status_code status_code, std::string reason_phrase) : http_response(version, static_cast<unsigned int>(status_code), std::move(reason_phrase)) {}

	http_response::http_response(http_version version, http_status_code status_code, std::string reason_phrase, header_map map) : http_response(version, static_cast<unsigned int>(status_code), std::move(reason_phrase), std::move(map)) {}



	bool is_ctl(int ch) {
		return (ch >= 0 && ch <= 31) || ch == 127;
	}

	bool is_separator(int ch) {
		switch (ch) {
		case '(':
			return true;
		case ')':
			return true;
		case '<':
			return true;
		case '>':
			return true;
		case '@':
			return true;
		case ',':
			return true;
		case ';':
			return true;
		case ':':
			return true;
		case '\\':
			return true;
		case '"':
			return true;
		case '/':
			return true;
		case '[':
			return true;
		case ']':
			return true;
		case '?':
			return true;
		case '=':
			return true;
		case '{':
			return true;
		case '}':
			return true;
		case ' ':
			return true;
		case '\t':
			return true;
		default:
			return false;
		}
	}

	bool is_token(int ch) {
		return !is_ctl(ch) && !is_separator(ch) && ch >= 0 && ch <= 127;
	}

	bool is_crlf(int ch) {
		return ch == '\r' || ch == '\n';
	}

	future<int> get_ch(istream& stream) {
		return stream.get().and_then<int>([](optional<int> ch) {
			if (!ch) {
				throw http_error(http_status_code::bad_request);
			} else {
				return resolve(std::move(*ch));
			}
		});
	}
}
