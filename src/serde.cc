#include "cobra/serde.hh"

#include <cstring>

namespace cobra {
	task<void> write_u8(ostream_reference os, std::uint8_t val) {
		char buffer[1];
		std::memcpy(buffer, &val, 1);
		co_await os.write_all(buffer, 1);
	}

	task<std::uint8_t> read_u8(istream_reference is) {
		char buffer[1];
		std::uint8_t ret;
		co_await is.read_all(buffer, 1);
		std::memcpy(&ret, buffer, 1);
		co_return ret;
	}

	task<void> write_u16_be(ostream_reference os, std::uint16_t val) {
		co_await write_u8(os, val >> 8 & 0xFF);
		co_await write_u8(os, val >> 0 & 0xFF);
	}

	task<void> write_u32_be(ostream_reference os, std::uint32_t val) {
		co_await write_u8(os, val >> 24 & 0xFF);
		co_await write_u8(os, val >> 16 & 0xFF);
		co_await write_u8(os, val >> 8 & 0xFF);
		co_await write_u8(os, val >> 0 & 0xFF);
	}

	task<void> write_u64_be(ostream_reference os, std::uint64_t val) {
		co_await write_u8(os, val >> 56 & 0xFF);
		co_await write_u8(os, val >> 48 & 0xFF);
		co_await write_u8(os, val >> 40 & 0xFF);
		co_await write_u8(os, val >> 32 & 0xFF);
		co_await write_u8(os, val >> 24 & 0xFF);
		co_await write_u8(os, val >> 16 & 0xFF);
		co_await write_u8(os, val >> 8 & 0xFF);
		co_await write_u8(os, val >> 0 & 0xFF);
	}

	task<std::uint16_t> read_u16_be(istream_reference is) {
		std::uint16_t ret = 0;
		ret |= static_cast<std::uint16_t>(co_await read_u8(is)) << 8;
		ret |= static_cast<std::uint16_t>(co_await read_u8(is)) << 0;
		co_return ret;
	}

	task<std::uint32_t> read_u32_be(istream_reference is) {
		std::uint32_t ret = 0;
		ret |= static_cast<std::uint32_t>(co_await read_u8(is)) << 24;
		ret |= static_cast<std::uint32_t>(co_await read_u8(is)) << 16;
		ret |= static_cast<std::uint32_t>(co_await read_u8(is)) << 8;
		ret |= static_cast<std::uint32_t>(co_await read_u8(is)) << 0;
		co_return ret;
	}

	task<std::uint64_t> read_u64_be(istream_reference is) {
		std::uint64_t ret = 0;
		ret |= static_cast<std::uint64_t>(co_await read_u8(is)) << 56;
		ret |= static_cast<std::uint64_t>(co_await read_u8(is)) << 48;
		ret |= static_cast<std::uint64_t>(co_await read_u8(is)) << 40;
		ret |= static_cast<std::uint64_t>(co_await read_u8(is)) << 32;
		ret |= static_cast<std::uint64_t>(co_await read_u8(is)) << 24;
		ret |= static_cast<std::uint64_t>(co_await read_u8(is)) << 16;
		ret |= static_cast<std::uint64_t>(co_await read_u8(is)) << 8;
		ret |= static_cast<std::uint64_t>(co_await read_u8(is)) << 0;
		co_return ret;
	}

	task<void> write_u16_le(ostream_reference os, std::uint16_t val) {
		co_await write_u8(os, val >> 0 & 0xFF);
		co_await write_u8(os, val >> 8 & 0xFF);
	}

	task<void> write_u32_le(ostream_reference os, std::uint32_t val) {
		co_await write_u8(os, val >> 0 & 0xFF);
		co_await write_u8(os, val >> 8 & 0xFF);
		co_await write_u8(os, val >> 16 & 0xFF);
		co_await write_u8(os, val >> 24 & 0xFF);
	}

	task<void> write_u64_le(ostream_reference os, std::uint64_t val) {
		co_await write_u8(os, val >> 0 & 0xFF);
		co_await write_u8(os, val >> 8 & 0xFF);
		co_await write_u8(os, val >> 16 & 0xFF);
		co_await write_u8(os, val >> 24 & 0xFF);
		co_await write_u8(os, val >> 32 & 0xFF);
		co_await write_u8(os, val >> 40 & 0xFF);
		co_await write_u8(os, val >> 48 & 0xFF);
		co_await write_u8(os, val >> 56 & 0xFF);
	}

	task<std::uint16_t> read_u16_le(istream_reference is) {
		std::uint16_t ret = 0;
		ret |= static_cast<std::uint16_t>(co_await read_u8(is)) << 0;
		ret |= static_cast<std::uint16_t>(co_await read_u8(is)) << 8;
		co_return ret;
	}

	task<std::uint32_t> read_u32_le(istream_reference is) {
		std::uint32_t ret = 0;
		ret |= static_cast<std::uint32_t>(co_await read_u8(is)) << 0;
		ret |= static_cast<std::uint32_t>(co_await read_u8(is)) << 8;
		ret |= static_cast<std::uint32_t>(co_await read_u8(is)) << 16;
		ret |= static_cast<std::uint32_t>(co_await read_u8(is)) << 24;
		co_return ret;
	}

	task<std::uint64_t> read_u64_le(istream_reference is) {
		std::uint64_t ret = 0;
		ret |= static_cast<std::uint64_t>(co_await read_u8(is)) << 0;
		ret |= static_cast<std::uint64_t>(co_await read_u8(is)) << 8;
		ret |= static_cast<std::uint64_t>(co_await read_u8(is)) << 16;
		ret |= static_cast<std::uint64_t>(co_await read_u8(is)) << 24;
		ret |= static_cast<std::uint64_t>(co_await read_u8(is)) << 32;
		ret |= static_cast<std::uint64_t>(co_await read_u8(is)) << 40;
		ret |= static_cast<std::uint64_t>(co_await read_u8(is)) << 48;
		ret |= static_cast<std::uint64_t>(co_await read_u8(is)) << 56;
		co_return ret;
	}
} // namespace cobra
