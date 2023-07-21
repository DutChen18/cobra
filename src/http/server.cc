#include "cobra/http/server.hh"

#include "cobra/asyncio/stream_buffer.hh"
#include "cobra/config.hh"
#include "cobra/http/handler.hh"
#include "cobra/http/parse.hh"

#include <exception>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <queue>
#include <ranges>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <variant>

namespace cobra {

	http_filter::http_filter(std::shared_ptr<const config::config> config, std::size_t match_count)
		: _config(config), _match_count(match_count + config->location.size()) {
		for (const auto& sub_filter : _config->sub_configs) {
			_sub_filters.push_back(http_filter(sub_filter, _match_count));
		}
	}

	http_filter::http_filter(std::shared_ptr<const config::config> config) : http_filter(config, 0) {}

	http_filter::http_filter(std::shared_ptr<const config::config> config, std::vector<http_filter> filters)
		: _config(config), _sub_filters(std::move(filters)) , _match_count(0) {}

	http_filter* http_filter::match(const http_request& request, const uri_abs_path& normalized) {
		if (eval(request, normalized)) {
			for (auto& filter : _sub_filters) {
				auto m = filter.match(request, normalized);
				if (m)
					return m;
			}
			return this;
		}
		return nullptr;
	}

	bool http_filter::eval(const http_request& request, const uri_abs_path& normalized) const {
		if (!config().server_names.empty()) {
			if (!request.has_header("host")) {
				eprintln("no host header");
				return false;
			}
			if (!config().server_names.contains(request.header("host"))) {
				eprintln("other host");
				return false;
			}
		}

		if (!config().location.empty()) {
			const std::size_t already_matched = match_count() - config().location.size();

			auto it = normalized.begin() + already_matched;

			for (auto& part : config().location) {
					if (it == normalized.end()) {
						eprintln("too short, expected: {}", part);
						return false;
					} else if (*it != part) {
						eprintln("{} != {}", *it, part);
						return false;
					}
				++it;
			}
		}

		if (!config().methods.empty() && !config().methods.contains(request.method())) {
			eprintln("other method");
			return false;
		}
		return true;
	}

	server::server(config::listen_address address, std::vector<http_filter> filters, executor* exec, event_loop* loop)
		: http_filter(std::shared_ptr<config::config>(new config::config()), std::move(filters)),
		  _address(std::move(address)), _exec(exec), _loop(loop) {}

	task<void> server::on_connect(socket_stream socket) {
		std::cout << "connect" << std::endl;
		istream_buffer socket_istream(make_istream_ref(socket), 1024);
		ostream_buffer socket_ostream(make_ostream_ref(socket), 1024);

		std::optional<http_parse_error> error;
		bool unknown_error = false;

		try {
			http_request request = co_await parse_http_request(socket_istream);

			const uri_origin* org = request.uri().get<uri_origin>();
			if (!org) {
				eprintln("request did not contain uri_origin");
				co_return co_await write_http_response(socket_ostream, http_response({1,1}, 400, "Bad request"));
			}
			const uri_abs_path normalized = org->path().normalize();

			auto filter = match(request, normalized);

			if (!filter || !filter->config().handler) {
				std::cerr << "no filter or handler found" << std::endl;
				co_return co_await write_http_response(socket_ostream, http_response({1, 1}, 404, "Not found"));
			}

			co_await handle_request(*filter, request, normalized, socket_istream, socket_ostream);
			// co_await (*handler)(std::move(writer), std::move(request), buffered_istream_reference(socket_istream));
			// TODO limit socket_istream to client content-length
		} catch (http_parse_error err) {
			error = std::move(err);
		} catch (const std::exception& ex) {
			unknown_error = true;
			std::cerr << ex.what() << std::endl;
		} catch (...) {
			unknown_error = true;
			std::cerr << "something went very very wrong" << std::endl;
		}

		if (error) {
			co_await write_http_response(socket_ostream, http_response({1, 1}, 400, "Bad request"));
		} else if (unknown_error) {
			co_await write_http_response(socket_ostream, http_response({1, 1}, 500, "Internal server error"));
		}
	}

	task<void> server::handle_request(const http_filter& filt, const http_request& request, const uri_abs_path& normalized,
									  buffered_istream_reference in, buffered_ostream_reference out) {
		http_response_writer writer(out);
		eprintln("match count: {}", filt.match_count());
		// TODO write headers set in config
		std::cout << "path: " << request.uri().string() << std::endl;
		// TODO properly match uri
		//
		fs::path file("/");
		for (std::size_t i = filt.match_count(); i < normalized.size(); ++i) {
			file.append(normalized[i]);
		}

		co_await handle_static(std::move(writer),
							   {_loop,
								_exec,
								file.string(),
								{std::get<config::static_file_config>(*filt.config().handler).root.path},
								request,
								in});
	}

	task<void> server::start(executor* exec, event_loop* loop) {
		std::string service = std::to_string(_address.service());
		std::cout << _address.node() << ":" << service << std::endl;
		co_return co_await start_server(exec, loop, _address.node().data(), service.c_str(),
										[this](socket_stream socket) -> task<void> {
											co_return co_await on_connect(std::move(socket));
										});
	}

	std::vector<server> server::convert(std::shared_ptr<const config::server> config, executor* exec, event_loop *loop) {
		std::map<config::listen_address, std::vector<http_filter>> filters;

		for (const auto& address : config->addresses) {
			filters[address].push_back(http_filter(config));
		}

		std::vector<server> result;
		result.reserve(filters.size());
		for (const auto& [listen, filters] : filters) {
			result.push_back(server(listen, filters, exec, loop));
		}
		return result;
	}

	std::vector<server> server::convert(const std::vector<std::shared_ptr<config::server>>& configs,
										executor* exec, event_loop* loop) {
		//TODO ssl
		std::map<config::listen_address, std::vector<http_filter>> filters;

		for (const auto& config : configs) {
			for (const auto& address : config->addresses) {
				filters[address].push_back(http_filter(config));
			}

		}

		std::vector<server> result;
		result.reserve(filters.size());
		for (const auto& [listen, filters] : filters) {
			result.push_back(server(listen, filters, exec, loop));
		}
		return result;
	}
} // namespace cobra
