#include "cobra/asio.hh"

extern "C" {
#include <stdint.h>
#include <stddef.h>
}

namespace cobra {
	class istringstream : public cobra::basic_istream<char> {
		const uint8_t *_data;
		std::size_t _size;

	public:
		istringstream(const uint8_t *data, size_t size) : _data(data), _size(size) {}

		future<std::size_t> read(
	};
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {

}
