#include "cobra/server.hh"
#include "cobra/file.hh"
#include "cobra/path.hh"

namespace cobra {
	static_request_handler::static_request_handler(std::string root) : _root(std::move(root)) {
	}

	cgi_request_handler::cgi_request_handler(std::string cgi_path, std::string script_path) : _cgi_path(std::move(cgi_path)), _script_path(std::move(script_path)) {
	}

	future<bool> static_request_handler::handle(const request_info& info) {
		std::shared_ptr<fstream> file;
		http_response response(http_version(1, 1), 200, "OK");
	   
		try {
			file = std::make_shared<fstream>(path(_root).mount(info.path_info()), std::fstream::in);
		} catch (const std::ios_base::failure&) {
			return resolve(false);
		}

		return write_response(info.ostream(), response).and_then<bool>([file, &info](unit) {
			return pipe(*file, info.ostream()).and_then<bool>([file](unit) {
				return resolve(true);
			});
		});
	}

	future<bool> cgi_request_handler::handle(const request_info& info) {

	}
}
