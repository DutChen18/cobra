#include "cobra/http.hh"

#include <sstream>

namespace cobra {
	static const char* get_status_text(unsigned int status_code) {
		switch (status_code) {
		case static_cast<unsigned int>(http_status_code::ok):
			return "OK";
		case static_cast<unsigned int>(http_status_code::bad_request):
			return "Bad Request";
		case static_cast<unsigned int>(http_status_code::content_too_large):
			return "Payload Too Large";
		case static_cast<unsigned int>(http_status_code::uri_too_long):
			return "URI Too Long";
		case static_cast<unsigned int>(http_status_code::request_header_fields_too_large):
			return "Request Header Fields Too Large";
		case static_cast<unsigned int>(http_status_code::not_implemented):
			return "Not Implemented";
		default:
			return "Unknown";
		}
	}

	static std::ostream& operator<<(std::ostream& os, http_version version) {
		return os << "HTTP/" << version.major() << "." << version.minor();
	}

	std::ostream& operator<<(std::ostream& os, const http_request& request) {
		os << request.method() << " ";
		os << request.request_uri() << " ";
		os << request.version() << "\r\n";
		return os << request.headers();
	}

	std::ostream& operator<<(std::ostream& os, const http_response& response) {
		os << response.version() << " ";
		os << (response.status_code() / 100 % 10);
		os << (response.status_code() / 10 % 10);
		os << (response.status_code() / 1 % 10) << " ";
		os << get_status_text(response.status_code()) << "\r\n";
		return os << response.headers();
	}
	
	future<unit> write_request(ostream& stream, const http_request& request) {
		std::unique_ptr<std::string> data = make_unique<std::string>((std::stringstream() << request).str());
		return stream.write_all(data->data(), data->size()).and_then<unit>(capture([](std::unique_ptr<std::string>&, std::size_t) {
			return resolve(unit());
		}, std::move(data)));
	}

	future<unit> write_response(ostream& stream, const http_response& response) {
		std::unique_ptr<std::string> data = make_unique<std::string>((std::stringstream() << response).str());
		return stream.write_all(data->data(), data->size()).and_then<unit>(capture([](std::unique_ptr<std::string>&, std::size_t) {
			return resolve(unit());
		}, std::move(data)));
	}
}
