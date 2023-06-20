#include "cobra/server.hh"
#include "cobra/file.hh"
#include "cobra/path.hh"
#include "cobra/process.hh"

namespace cobra {
	future<std::size_t> request_info::read(char_type* dst, std::size_t count) {
		return _istream->read(dst, count);
	}

	future<std::size_t> request_info::write(const char_type* data, std::size_t count) {
		if (!_response_sent) {
			return send_response().and_then<std::size_t>([this, data, count](unit) {
				return _ostream->write(data, count);
			});
		} else {
			return _ostream->write(data, count);
		}
	}

	future<unit> request_info::flush() {
		return _ostream->flush();
	}

	future<unit> request_info::send_response() {
		if (_response_sent) {
			std::cerr << "attempt to send response twice" << std::endl;
		} else {
			return write_response(*_ostream, response());
		}

		_response_sent = true;
		return resolve(unit());
	}

	future<unit> request_info::end() {
		_ostream.reset();
		return resolve(unit());
	}

	future<unit> request_info::end(istream& is) {
		return pipe(is, *this).and_then<unit>([this](unit) {
			return end();
		});
	}

	static_request_handler::static_request_handler(std::string root) : _root(std::move(root)) {
	}

	cgi_request_handler::cgi_request_handler(std::string cgi_path, std::string script_path) : _cgi_path(std::move(cgi_path)), _script_path(std::move(script_path)) {
	}

	future<bool> static_request_handler::handle(request_info& info, const std::string& path_info) {
		std::shared_ptr<fstream> file;
	   
		try {
			file = std::make_shared<fstream>(_root.mount(path_info), std::fstream::in);
		} catch (const std::ios_base::failure&) {
			return resolve(false);
		}

		return info.end(*file).and_then<bool>([file](unit) {
			return resolve(true);
		});
	}
	
	static optional<std::pair<path, path>> get_path_info(path p) {
		std::vector<std::string> path_info;

		while (!p.empty()) {
			if (std::ifstream(p).good()) {
				return some<std::pair<path, path>>(p, path(path_type::absolute, path_info));
			} else {
				path_info.push_back(p.basename());
				p = p.dirname();
			}
		}

		return none<std::pair<path, path>>();
	}

	optional<command> cgi_request_handler::get_command(const request_info& info, const std::string& relative_path_info) const {
		path absolute_path_info = _script_path.mount(relative_path_info);
		optional<std::pair<path, path>> path_info = get_path_info(absolute_path_info);

		if (!path_info) {
			return none<command>();
		}
		
		command cmd({ _cgi_path, path_info->first });

		cmd.set_in(fd_mode::pipe);
		cmd.set_out(fd_mode::pipe);

		cmd.insert_env("PATH_INFO", path_info->second);
		cmd.insert_env("REQUEST_METHOD", info.request().method());
		cmd.insert_env("SCRIPT_FILENAME", path_info->first);
		cmd.insert_env("REDIRECT_STATUS", "200");
		// TODO: more vars

		if (info.request().headers().contains("Content-Length"))
			cmd.insert_env("CONTENT_LENGTH", info.request().headers().at("Content-Length"));
		if (info.request().headers().contains("Content-Type"))
			cmd.insert_env("CONTENT_TYPE", info.request().headers().at("Content-Type"));

		for (const auto& header : info.request().headers()) {
			std::string key = "HTTP_";

			for (char ch : header.first) {
				key.push_back(ch == '-' ? '_' : std::toupper(ch));
			}

			cmd.insert_env(key, header.second);
		}

		return some<command>(cmd);
	}

	future<bool> cgi_request_handler::handle(request_info& info, const std::string& path_info) {
		optional<command> cmd = get_command(info, path_info);

		if (!cmd) {
			return resolve(false);
		}

		std::shared_ptr<process> proc = std::make_shared<process>(cmd->spawn());
		std::shared_ptr<buffered_istream> proc_istream = std::make_shared<buffered_istream>(std::make_shared<fd_istream>(std::move(proc->out)));
		std::shared_ptr<buffered_ostream> proc_ostream = std::make_shared<buffered_ostream>(std::make_shared<fd_ostream>(std::move(proc->in)));

		return all(
			pipe(info, *proc_ostream),
			parse_cgi_headers(*proc_istream).and_then<unit>([&info, proc_istream](header_map) {
				http_response response = info.response();

				// TODO: set response stuff

				return write_response(info, response).and_then<unit>([&info, proc_istream](unit) {
					return pipe(*proc_istream, info).and_then<unit>([](unit) {
						return resolve(unit());
					});
				});
			})
		).and_then<bool>([proc, proc_istream, proc_ostream](std::tuple<unit, unit>) {
			return proc->wait().and_then<bool>([proc, proc_istream, proc_ostream](int) {
				return resolve(true);
			});
		});
	}
}
