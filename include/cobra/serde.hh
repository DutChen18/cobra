#ifndef COBRA_SERDE_HH
#define COBRA_SERDE_HH

#include "cobra/asyncio/stream.hh"

#include <cstdint>

namespace cobra {
	task<void> write_u8(ostream_reference os, std::uint8_t val);
	task<std::uint8_t> read_u8(istream_reference is);

	task<void> write_u16_be(ostream_reference os, std::uint16_t val);
	task<void> write_u32_be(ostream_reference os, std::uint32_t val);
	task<void> write_u64_be(ostream_reference os, std::uint64_t val);

	task<std::uint16_t> read_u16_be(istream_reference is);
	task<std::uint32_t> read_u32_be(istream_reference is);
	task<std::uint64_t> read_u64_be(istream_reference is);

	task<void> write_u16_le(ostream_reference os, std::uint16_t val);
	task<void> write_u32_le(ostream_reference os, std::uint32_t val);
	task<void> write_u64_le(ostream_reference os, std::uint64_t val);

	task<std::uint16_t> read_u16_le(istream_reference is);
	task<std::uint32_t> read_u32_le(istream_reference is);
	task<std::uint64_t> read_u64_le(istream_reference is);
}

#endif
