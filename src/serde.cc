#include "cobra/serde.hh"

#include <cstring>

namespace cobra {
	task<void> write_u8_be(ostream_reference os, std::uint8_t val) {
		char buffer[1];
		std::memcpy(buffer, &val, 1);
		co_await os.write_all(buffer, 1);
	}

	task<void> write_u16_be(ostream_reference os, std::uint16_t val) {
		co_await write_u8_be(os, val >> 8 & 0xFF);
		co_await write_u8_be(os, val >> 0 & 0xFF);
	}

	task<void> write_u32_be(ostream_reference os, std::uint32_t val) {
		co_await write_u8_be(os, val >> 24 & 0xFF);
		co_await write_u8_be(os, val >> 16 & 0xFF);
		co_await write_u8_be(os, val >> 8 & 0xFF);
		co_await write_u8_be(os, val >> 0 & 0xFF);
	}

	task<void> write_u64_be(ostream_reference os, std::uint64_t val) {
		co_await write_u8_be(os, val >> 56 & 0xFF);
		co_await write_u8_be(os, val >> 48 & 0xFF);
		co_await write_u8_be(os, val >> 40 & 0xFF);
		co_await write_u8_be(os, val >> 32 & 0xFF);
		co_await write_u8_be(os, val >> 24 & 0xFF);
		co_await write_u8_be(os, val >> 16 & 0xFF);
		co_await write_u8_be(os, val >> 8 & 0xFF);
		co_await write_u8_be(os, val >> 0 & 0xFF);
	}

	task<std::uint8_t> read_u8_be(istream_reference is) {
		char buffer[1];
		std::uint8_t ret;
		co_await is.read_all(buffer, 1);
		std::memcpy(&ret, buffer, 1);
		co_return ret;
	}

	task<std::uint16_t> read_u16_be(istream_reference is) {
		std::uint16_t ret = 0;
		ret |= static_cast<std::uint16_t>(co_await read_u8_be(is)) << 8;
		ret |= static_cast<std::uint16_t>(co_await read_u8_be(is)) << 0;
		co_return ret;
	}

	task<std::uint32_t> read_u32_be(istream_reference is) {
		std::uint32_t ret = 0;
		ret |= static_cast<std::uint32_t>(co_await read_u8_be(is)) << 24;
		ret |= static_cast<std::uint32_t>(co_await read_u8_be(is)) << 16;
		ret |= static_cast<std::uint32_t>(co_await read_u8_be(is)) << 8;
		ret |= static_cast<std::uint32_t>(co_await read_u8_be(is)) << 0;
		co_return ret;
	}

	task<std::uint64_t> read_u64_be(istream_reference is) {
		std::uint64_t ret = 0;
		ret |= static_cast<std::uint64_t>(co_await read_u8_be(is)) << 56;
		ret |= static_cast<std::uint64_t>(co_await read_u8_be(is)) << 48;
		ret |= static_cast<std::uint64_t>(co_await read_u8_be(is)) << 40;
		ret |= static_cast<std::uint64_t>(co_await read_u8_be(is)) << 32;
		ret |= static_cast<std::uint64_t>(co_await read_u8_be(is)) << 24;
		ret |= static_cast<std::uint64_t>(co_await read_u8_be(is)) << 16;
		ret |= static_cast<std::uint64_t>(co_await read_u8_be(is)) << 8;
		ret |= static_cast<std::uint64_t>(co_await read_u8_be(is)) << 0;
		co_return ret;
	}
}
