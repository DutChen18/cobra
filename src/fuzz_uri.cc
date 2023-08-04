#include <cstdint>
#include <memory>
#include <string>
#include <sstream>
#include "cobra/http/parse.hh"
#include <cstddef>

#ifdef COBRA_FUZZ_URI

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	std::string_view str(reinterpret_cast<const char*>(data), size);
	using namespace cobra;

	try {
		parse_uri(str, "GET");
		parse_uri(str, "CONNECT");
	} catch (uri_parse_error) {
	}
	return 0;
}
#endif
