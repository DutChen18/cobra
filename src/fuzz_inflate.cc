#ifdef COBRA_FUZZ_INFLATE
#include <cstdint>
#include <memory>
#include <string>
#include <sstream>
#include "cobra/http/parse.hh"
#include <cstddef>
#include "cobra/asyncio/std_stream.hh"
#include "cobra/asyncio/stream_buffer.hh"
#include "cobra/asyncio/deflate.hh"
#include "cobra/asyncio/future_task.hh"


extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	std::stringstream stream(std::string(reinterpret_cast<const char*>(data), size));
	using namespace cobra;

	auto cobra_stream = std_istream<std::stringstream>(std::move(stream));
	//auto cobra_stream_buf = istream_buffer(std::move(cobra_stream), 1024);
	auto inf_stream = inflate_istream(std::move(cobra_stream));

	try {
		char buffer[1024];
		while (true) {
			auto nread = block_task(inf_stream.read(buffer, sizeof(buffer)));
			if (nread == 0)
				break;
		}
	} catch (inflate_error) {
	} catch (stream_error) {
	}

	return 0;
}
#endif
