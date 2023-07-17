#include "cobra/http/server.hh"

#include <exception>
#include <queue>
#include <unordered_map>
#include "cobra/asyncio/stream_buffer.hh"
#include "cobra/config.hh"

namespace cobra {

	http_handler::http_handler(config::block_config config) : _filter(), _config(std::move(config)) {}
	http_handler::http_handler(config::filter filter, config::block_config config) : _filter(std::move(filter)), _config(std::move(config)) {
		for (auto&& sub_filter : config.filters()) {
			_sub_handlers.push_back(http_handler(sub_filter.first, std::move(sub_filter.second)));
		}
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

	server_handler::server_handler(port port, config::server_config config) : http_handler(std::move(config)), _port(port), _server_name(config.server_name()) {}

	bool server_handler::eval(const socket_stream& socket, const http_request& request) const {
		if (!_server_name.empty()) {
			//TODO test host header
			//if (request.
		}
		//TODO match socket port
		return true;
	}

	task<void> server::on_connect(socket_stream socket) {
		istream_buffer socket_istream(make_istream_ref(socket), 1024);
		ostream_buffer socket_ostream(make_ostream_ref(socket), 1024);

		const http_request request = co_await parse_request(socket_istream);
		auto handler = match(socket, request);

		if (!handler) {
			//TODO send 404 not found
		}

		http_response_writer writer(socket_ostream);

		try {
			//TODO limit socket_istream to client content-length
			//(*handler)(std::move(writer), request, socket_istream);
		} catch (const std::exception &ex) {

		} catch (...) {

		}
	}

}
