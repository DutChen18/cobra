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

namespace cobra {

	http_handler::http_handler(config::block_config config) : _filter(), _config(std::move(config)) {}
	http_handler::http_handler(config::filter filter, config::block_config config) : _filter(std::move(filter)), _config(std::move(config)), _sub_handlers() {
		for (auto&& sub_filter : config.filters()) {
			_sub_handlers.push_back(http_handler(sub_filter.first, std::move(sub_filter.second)));
		}
	}

	task<void> http_handler::operator()(http_response_writer writer, const http_request& request, http_istream stream) {
		std::cerr << "imagine something happening" << std::endl;
	}

	http_handler::~http_handler() {}

	std::optional<std::reference_wrapper<http_handler>> http_handler::match(const socket_stream& socket, const http_request& request) {
		if (eval(socket, request)) {
			for (auto&& handler : _sub_handlers) {
				auto m = handler.match(socket, request);
				if (m)
					return m;
			}
			return *this;
		}
		return std::nullopt;
	}

	bool http_handler::eval(const socket_stream& /*unused*/, const http_request& request) const {
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

	server_handler::server_handler(config::server_config config) : http_handler(std::move(config)),  _server_name(config.server_name()) {}

	bool server_handler::eval(const socket_stream& socket, const http_request& request) const {
		(void) socket;
		return _server_name.empty() || request.header("host") == _server_name;
	}

	server::server(config::listen_address address, std::vector<std::unique_ptr<http_handler>> handlers) : _address(std::move(address)), _handlers(std::move(handlers)) {}

	task<void> server::on_connect(socket_stream socket) {
		std::cout << "connect" << std::endl;
		istream_buffer socket_istream(make_istream_ref(socket), 1024);
		ostream_buffer socket_ostream(make_ostream_ref(socket), 1024);

		std::optional<http_parse_error> error;
		bool unknown_error = false;

		try {
			http_request request = co_await parse_http_request(socket_istream);
			auto handler = match(socket, request);

			if (!handler)
				co_return co_await write_http_response(socket_ostream, http_response({1,1}, 404, "Not found"));

			http_response_writer writer(socket_ostream);

			co_await (*handler)(std::move(writer), std::move(request), buffered_istream_reference(socket_istream));
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

	std::vector<server> server::convert(const config::server_config& config) {
		std::map<config::listen_address, std::vector<std::unique_ptr<http_handler>>> handlers;

		for (auto&& address : config.addresses()) {
			handlers[address].push_back(std::unique_ptr<http_handler>(new server_handler(config)));
		}

		std::vector<server> result;
		result.reserve(handlers.size());
		for (auto&& [listen, handlers] : handlers) {
			result.push_back(server(std::move(listen), std::move(handlers)));
		}
		return result;
	}

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
	}

	std::optional<std::reference_wrapper<http_handler>> server::match(const socket_stream& socket, const http_request& request) {
		for (auto& handler : _handlers) {
			const auto res = handler->match(socket, request);
			if (res)
				return res;
		}
		return std::nullopt;
	}
}
