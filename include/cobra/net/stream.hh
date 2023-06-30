#ifndef COBRA_NET_STREAM_HH
#define COBRA_NET_STREAM_HH

#include "cobra/asyncio/event_loop.hh"
#include "cobra/asyncio/stream.hh"

#include <functional>

namespace cobra {
	class socket_stream : public istream, public ostream {
		event_loop* _loop;
		file _file;

	public:
		socket_stream(event_loop* loop, file&& f);
		socket_stream(socket_stream&& other);

		task<std::size_t> read(char_type* data, std::size_t size) override;
		task<std::size_t> write(const char_type* data, std::size_t size) override;
		task<void> flush() override;
	};

	task<socket_stream> open_connection(event_loop* loop, const char* node, const char* service);
	task<void> start_server(event_loop* loop, const char* node, const char* service,
							std::function<task<void>(socket_stream)> cb);
} // namespace cobra

#endif
