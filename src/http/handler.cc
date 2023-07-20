#include "cobra/http/handler.hh"
#include "cobra/asyncio/std_stream.hh"

#include <fstream>

namespace cobra {
	const std::filesystem::path& static_config::root() const {
		return _root;
	}

	// TODO: 404 not found
	task<void> handle_static(http_response_writer writer, const handle_context<static_config>& context) {
		std::filesystem::path path = context.config().root() / context.file();
		istream_buffer file_istream(std_istream(std::fstream(path)), 1024);
		http_ostream sock_ostream = co_await std::move(writer).send(HTTP_OK);
		co_await pipe(buffered_istream_reference(file_istream), ostream_reference(sock_ostream));
	}
}
