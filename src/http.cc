#include "cobra/http.hh"
#include "cobra/future.hh"
#include <memory>

namespace cobra {

	http_method parse_method(const std::string& str) {
		if (str == "OPTIONS") {
			return http_method::options;
		} else if (str == "GET") {
			return http_method::get;
		} else if (str == "HEAD") {
			return http_method::head;
		} else if (str == "POST") {
			return http_method::post;
		} else if (str == "PUT") {
			return http_method::put;
		} else if (str == "DEL") {
			return http_method::del;
		} else if (str == "TRACE") {
			return http_method::trace;
		} else if (str == "CONNECT") {
			return http_method::connect;
		}
		return http_method::unknown;
	}

        /*
        future<http_method> parse_method(buffered_isstream& stream) {
            return async_while([&stream]() {
                (void)stream;
                return resolve<optional<http_method>>(none<http_method>());
            });
        }

	future<http_method> parse_method(std::shared_ptr<istream> stream) {
		std::shared_ptr<http_method> method = std::make_shared<http_method>(http_method::unknown);
		return cobra::async_while([stream, method]() {
			std::shared_ptr<std::string> data(new std::string());

			return stream->get().then<bool>([stream, data, method](char ch) {
				if (ch == ' ')
					throw http_error(http_status_code::not_implemented);
				data->push_back(ch);

				http_method tmp = parse_method(*data);
				if (tmp != http_method::unknown) {
					*method = tmp;
					return false;
				} else if (data->length() > 16) {
					throw new http_error(http_status_code::not_implemented);
				}
				return true;
			});
		}).then<http_method>([method]() {
			return *method;
		});
	}

	future<std::string> parse_request_uri(std::shared_ptr<istream> stream) {
		std::shared_ptr<std::string> request_uri(new std::string());
		return cobra::async_while([stream, request_uri]() {
			return stream->get().then<bool>([stream, request_uri](char ch1) {
				return stream->peek().then<bool>([stream, request_uri, ch1](char ch2) {
					request_uri->push_back(ch1);
					if (ch2 == ' ') {
						return false;
					} else if (request_uri->length() >= 1024) {
						throw http_error(http_status_code::uri_too_long);
					}
					return true;
				});
			});
		}).then<std::string>([request_uri]() {
			return *request_uri;
		});
	}

	future<bool> expect_string(std::shared_ptr<istream> stream) {

	}

	future<http_version> parse_http_version(std::shared_ptr<istream> stream) {
		std::shared_ptr<http_version> version(new http_version());
		return cobra::async_while([stream, version]() {
		}).then<http_version>([version]() {
			return *version;
		});
	}*/

}
