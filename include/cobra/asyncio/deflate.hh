#ifndef COBRA_ASYNCIO_DEFLATE_HH
#define COBRA_ASYNCIO_DEFLATE_HH

#include "cobra/asyncio/stream.hh"
#include "cobra/serde.hh"

#include <vector>
#include <cstdint>

namespace cobra {
	template <AsyncInputStream Stream>
	class bit_istream {
		Stream _stream;
		std::size_t _count = 0;
		std::uint8_t _data;

	public:
		bit_istream(Stream&& stream) : _stream(std::move(stream)) {
		}

		task<std::uintmax_t> read_bits(std::size_t size) {
			std::uintmax_t value = 0;
			std::size_t index = 0;

			while (size > index) {
				if (_count % 8 == 0) {
					_data = co_await read_u8_be(_stream);
				}

				std::size_t offset = _count % 8;
				std::size_t count = std::min(8 - offset, size - index);
				value |= ((_data >> offset) & ((1 << count) - 1)) << index;
				index += count;
				_count += count;
			}

			co_return value;
		}

		Stream end()&& {
			return std::move(_stream);
		}
	};

	template <AsyncOutputStream Stream>
	class bit_ostream {
		Stream _stream;
		std::size_t _count = 0;
		std::uint8_t _data = 0;

	public:
		bit_ostream(Stream&& stream) : _stream(std::move(stream)) {
		}

		task<void> write_bits(std::uintmax_t value, std::size_t size) {
			while (size > 0) {
				std::size_t offset = _count % 8;
				std::size_t count = std::min(8 - offset, size);
				_data |= (value & ((1 << count) - 1)) << offset;
				value >>= count;
				size -= count;
				_count += count;

				if (_count % 8 == 0) {
					co_await write_u8_be(_stream, _data);
					_data = 0;
				}
			}
		}

		task<Stream> end()&& {
			if (_count % 8 != 0) {
				co_await write_u8_be(_stream, _data);
			}

			co_return std::move(_stream);
		}
	};

	template <AsyncInputStream Stream>
	class deflate_istream : public buffered_istream_impl<deflate_istream<Stream>> {
		bit_istream<Stream> _stream;

	public:
		deflate_istream(Stream&& stream) : _stream(std::move(stream)) {
		}

		Stream end()&& {
			return std::move(_stream).end();
		}
	};
}

#endif
