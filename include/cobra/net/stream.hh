#ifndef COBRA_NET_STREAM_HH
#define COBRA_NET_STREAM_HH

#include "cobra/asyncio/event_loop.hh"
#include "cobra/asyncio/stream.hh"

#include <functional>

namespace cobra {
	class socket_stream : public istream_impl<socket_stream>, public ostream_impl<socket_stream> {
		event_loop* _loop;
		file _file;

	public:
		socket_stream(event_loop* loop, file&& f);

		task<std::size_t> read(char_type* data, std::size_t size);
		task<std::size_t> write(const char_type* data, std::size_t size);
		task<void> flush();
		void shutdown(int how);

		inline void set_event_loop(event_loop* loop) {
			_loop = loop;
		}
	};

	task<socket_stream> open_connection(event_loop* loop, const char* node, const char* service);
	task<void> start_server(executor* exec, event_loop* loop, const char* node, const char* service,
							std::function<task<void>(socket_stream)> cb);
} // namespace cobra

#endif
