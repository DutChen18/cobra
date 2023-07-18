#ifndef COBRA_HANDLER_HH
#define COBRA_HANDLER_HH

#include "cobra/asyncio/stream.hh"
#include "cobra/http/writer.hh"

#include <filesystem>

namespace cobra {
	class static_config {
		std::filesystem::path _root;
	};

	template <class T>
	class handle_context {
		std::string _file;
		std::reference_wrapper<const T> _config;
		std::reference_wrapper<const http_request> _request;
		buffered_istream_reference _istream;

	public:
		const std::string& file();
	};

	task<void> handle_static(http_response_writer writer, const handle_context<static_config>& context);
}

#endif
