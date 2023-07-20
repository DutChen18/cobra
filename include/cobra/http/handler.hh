#ifndef COBRA_HANDLER_HH
#define COBRA_HANDLER_HH

#include "cobra/asyncio/stream.hh"
#include "cobra/http/writer.hh"

#include <filesystem>

namespace cobra {
	class static_config {
		std::filesystem::path _root;

	public:
		static_config(const std::filesystem::path& root) : _root(root) {}//TODO do properly
		const std::filesystem::path& root() const;
	};

	template <class T>
	class handle_context {
		std::string _file;
		std::reference_wrapper<const T> _config;
		std::reference_wrapper<const http_request> _request;
		buffered_istream_reference _istream;

	public:
		handle_context(std::string file, const T& config, const http_request& request, buffered_istream_reference istream) : _file(std::move(file)), _config(config), _request(request), _istream(istream) {}
		const std::string& file() const {
			return _file;
		}

		const T& config() const {
			return _config;
		}

		const http_request& request() const {
			return _request;
		}

		buffered_istream_reference istream() const {
			return _istream;
		}
	};

	task<void> handle_static(http_response_writer writer, const handle_context<static_config>& context);
}

#endif
