#include "cobra/server.hh"
#include <memory>
#include <utility>

namespace cobra {

	std::pair<request_handler*, std::size_t> http_block::best_match(const http_request& request) {
		if (!match(request))
			return std::make_pair(nullptr, 0);

		std::pair<request_handler*, std::size_t> best = std::make_pair(nullptr, 1);

		if (_default_handler)
			best.first = &*_default_handler;

		for (auto&& child : _children) {
			auto match = child->best_match(request);
			if ((match.second + 1) > best.second)
				best = match;
		}
		return best;
	}

	future<unit> http_server::on_connect(connected_socket sock) {
		return async_while<unit>(capture([this](connected_socket sock) {
			std::shared_ptr<connected_socket> socket = std::make_shared<connected_socket>(std::move(sock));
			std::shared_ptr<buffered_istream> istream = std::make_shared<buffered_istream>(socket);
			std::shared_ptr<buffered_ostream> ostream = std::make_shared<buffered_ostream>(socket);

			return parse_request(*istream).and_then<optional<unit>>([this, socket, istream, ostream](http_request request) {
				auto match = best_match(request);

				if (match.first) {
					return match.first->handle(istream, ostream, request).and_then<optional<unit>>([](bool close_connection) {
						if (close_connection)
							return resolve(some<unit>());
						return resolve(none<unit>());
					});
				}
				//TODO send not found
				return resolve(some<unit>());
			});
		}, std::move(sock)));
	}
}
