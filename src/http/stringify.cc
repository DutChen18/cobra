#include "cobra/http.hh"

#include <sstream>

namespace cobra {
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
		os << response.reason_phrase() << "\r\n";
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
