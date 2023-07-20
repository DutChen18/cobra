#include "cobra/http/server.hh"

#include <exception>
#include <iterator>
#include <map>
#include <memory>
#include <queue>
#include <ranges>
#include <unordered_map>
#include "cobra/asyncio/stream_buffer.hh"
#include "cobra/config.hh"
#include "cobra/http/parse.hh"
#include "cobra/http/handler.hh"

namespace cobra {

	http_filter::http_filter(config::config config) : http_filter(std::string(), std::move(config)) {}
	http_filter::http_filter(std::vector<http_filter> sub_filters) : _sub_filters(std::move(sub_filters)) {}

	http_filter::http_filter(const std::string& location, config::config config) : _filter(config.filter), _config(std::move(config)), _path(location) {
		std::string sub_location = _path;

		if (_filter && _filter->type == config::filter::type::location)
			sub_location.append(_filter->match);

		for (const auto& sub_filter : config.sub_configs) {
			_sub_filters.push_back(http_filter(sub_location, sub_filter));
		}
	}

	http_filter* http_filter::match(const http_request& request) {
		if (eval(request)) {
			for (auto& filter : _sub_filters) {
				auto m = filter.match(request);
				if (m)
					return m;
			}
			return this;
		}
		return nullptr;
	}

	bool http_filter::eval(const http_request& request) const {
		if (!config().server_names.empty()) {
			if (!request.has_header("host"))
				return false;
			if (!config().server_names.contains(request.header("host")))
				return false;
		}

		if (_filter) {
			switch (_filter->type) {
			case config::filter::type::method:
				return request.method() == _filter->match;
			case config::filter::type::location:
				return true; //TODO
			}
		}
		return true;
	}

	server::server(config::listen_address address, std::vector<http_filter> filters)
		: http_filter(std::move(filters)), _address(std::move(address)) {}

	task<void> server::on_connect(socket_stream socket) {
		std::cout << "connect" << std::endl;
		istream_buffer socket_istream(make_istream_ref(socket), 1024);
		ostream_buffer socket_ostream(make_ostream_ref(socket), 1024);

		std::optional<http_parse_error> error;
		bool unknown_error = false;

		try {
			http_request request = co_await parse_http_request(socket_istream);
			auto filter = match(request);

			if (!filter) {
				std::cerr << "no filter found" << std::endl;
				co_return co_await write_http_response(socket_ostream, http_response({1,1}, 404, "Not found"));
			}
			std::cerr << "found a filter" << std::endl;

			//http_response_writer writer(socket_ostream);

			co_await handle_request(*filter, request, socket_istream, socket_ostream);
			//co_await (*handler)(std::move(writer), std::move(request), buffered_istream_reference(socket_istream));
			//TODO limit socket_istream to client content-length
		} catch (http_parse_error err) {
			error = std::move(err);
		} catch (const std::exception &ex) {
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

	task<void> server::handle_request(const http_filter& filt, const http_request& request, buffered_istream_reference in, buffered_ostream_reference out) {
		http_response_writer writer(out);
		//TODO write headers set in config
		if (filt.config().handler) {
			std::cout << "path: " << filt.path() << std::endl;
			co_await handle_static(std::move(writer), {std::string(), {fs::path(filt.path())}, request, in});
			//TODO execute handler
		} else {
			//TODO send 404 not found
		}
		co_return;
	}

	task<void> server::start(executor* exec, event_loop *loop) {
		try {
			std::string service = std::to_string(_address.service());
			std::cout << _address.node() << ":" << service << std::endl;
			co_return co_await start_server(exec, loop, _address.node().data(), service.c_str(), [this](socket_stream socket) -> task<void> {
				co_return co_await on_connect(std::move(socket));
			});
		} catch (...) {
			std::terminate();
		}
	}

	std::vector<server> server::convert(const config::server& config) {
		std::map<config::listen_address, std::vector<http_filter>> filters;

		for (const auto& address : config.addresses) {
			filters[address].push_back(http_filter(config));
		}

		std::vector<server> result;
		result.reserve(filters.size());
		for (const auto& [listen, filters] : filters) {
			result.push_back(server(listen, filters));
		}
		return result;
	}

	/*
	std::vector<server> server::convert(const std::vector<config::server_config>& configs) {
		std::map<config::listen_address, std::vector<std::unique_ptr<http_handler>>> handlers;

		for (auto& config : configs) {
			for (auto&& address : config.addresses()) {
				handlers[address].push_back(std::unique_ptr<http_handler>(new server_handler(config)));
			}
		}

		std::vector<server> result;
		result.reserve(handlers.size());
		for (auto&& [listen, handlers] : handlers) {
			result.push_back(server(std::move(listen), std::move(handlers)));
		}
		return result;
	}*/
}
