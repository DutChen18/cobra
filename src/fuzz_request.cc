#include <cstdint>
#include <memory>
#include <string>
#include <sstream>
#include "cobra/http/message.hh"
#include "cobra/http/parse.hh"
#include "cobra/asyncio/future_task.hh"
#include "cobra/asyncio/std_stream.hh"
#include "cobra/asyncio/stream_buffer.hh"
#include <cstddef>

#ifdef COBRA_FUZZ_REQUEST

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	std::stringstream stream(std::string(reinterpret_cast<const char*>(data), size));
	using namespace cobra;

	try {
		auto cobra_stream = std_istream<std::stringstream>(std::move(stream));
		auto cobra_stream_buf = istream_buffer(std::move(cobra_stream), 1024);
		block_task(parse_http_request(cobra_stream_buf));

	} catch (http_parse_error) {
	} catch (uri_parse_error) {
	}
	return 0;
}
#endif
