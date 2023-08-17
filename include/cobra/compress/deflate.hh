#ifndef COBRA_COMPRESS_DEFLATE_HH
#define COBRA_COMPRESS_DEFLATE_HH

#include "cobra/compress/bit_stream.hh"
#include "cobra/compress/stream_ringbuffer.hh"
#include "cobra/compress/tree.hh"

#define COBRA_DEFLATE_NONE 0
#define COBRA_DEFLATE_FIXED 1
#define COBRA_DEFLATE_DYNAMIC 2

namespace cobra {
	using inflate_ltree = inflate_tree<std::uint16_t, 288, 15>;
	using inflate_dtree = inflate_tree<std::uint8_t, 30, 15>;
	using inflate_ctree = inflate_tree<std::uint8_t, 19, 7>;
	using deflate_ltree = deflate_tree<std::uint16_t, 288, 15>;
	using deflate_dtree = deflate_tree<std::uint8_t, 30, 15>;
	using deflate_ctree = deflate_tree<std::uint8_t, 19, 7>;

	constexpr std::array<std::size_t, 19> frobnication_table {
		16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15,
	};

	constexpr std::array<std::size_t, 288> inflate_fixed_tree {
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
		9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
		9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
		9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
		9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
		9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
		9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
		9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
		7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
		7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8,
	};

	template <AsyncInputStream Stream>
	class inflate_istream : public istream_ringbuffer<inflate_istream<Stream>> {
		using base = istream_ringbuffer<inflate_istream<Stream>>;

		struct state_init {
			bit_istream<Stream> stream;
		};

		struct state_write {
			Stream stream;
			std::size_t limit;
		};

		struct state_read {
			bit_istream<Stream> stream;
			inflate_ltree lt;
			std::optional<inflate_dtree> dt;
			std::size_t dist;
			std::size_t size;
		};

		std::variant<state_init, state_write, state_read> _state;
		bool _final = false;

		static task<std::uint16_t> decode(bit_istream<Stream>& stream, std::uint16_t code, std::uint16_t stride) {
			std::uint16_t extra_bits = code / stride;
			std::uint16_t block_offset = (stride << extra_bits) - stride;
			std::uint16_t start_offset = (code % stride) << extra_bits;
			co_return start_offset + block_offset + co_await stream.read_bits(extra_bits);
		}

		static task<std::size_t> decode_code(bit_istream<Stream>& stream, std::uint8_t code) {
			if (code == 16) {
				co_return co_await stream.read_bits(2) + 3;
			} else if (code == 17) {
				co_return co_await stream.read_bits(3) + 3;
			} else if (code == 18) {
				co_return co_await stream.read_bits(7) + 11;
			} else {
				co_return 1;
			}
		}

		static task<std::uint16_t> decode_size(bit_istream<Stream>& stream, std::uint16_t code) {
			if (code >= 286) {
				throw compress_error::bad_size_code;
			} else if (code == 285) {
				co_return 258;
			} else if (code < 261) {
				co_return code - 257 + 3;
			} else {
				co_return co_await decode(stream, code - 261, 4) + 7;
			}
		}

		static task<std::uint16_t> decode_dist(bit_istream<Stream>& stream, std::uint16_t code) {
			if (code >= 30) {
				throw compress_error::bad_dist_code;
			} else if (code < 2) {
				co_return code + 1;
			} else {
				co_return co_await decode(stream, code - 2, 2) + 3;
			}
		}

	public:
		inflate_istream(Stream&& stream) : base(32768), _state(state_init { bit_istream(std::move(stream)) }) {
		}

		task<void> fill_ringbuf() {
			while (!base::full()) {
				if (auto* state = std::get_if<state_init>(&_state)) {
					if (_final) {
						co_return;
					}

					_final = co_await state->stream.read_bits(1) == 1;
					int type = co_await state->stream.read_bits(2);

					if (type == COBRA_DEFLATE_NONE) {
						Stream stream = std::move(state->stream).end();
						std::uint16_t len = co_await read_u16_le(stream);
						std::uint16_t nlen = co_await read_u16_le(stream);

						if (len != static_cast<std::uint16_t>(~nlen)) {
							throw compress_error::bad_len_check;
						}

						_state = state_write { std::move(stream), len };
					} else if (type == COBRA_DEFLATE_FIXED) {
						inflate_ltree lt(inflate_fixed_tree.data(), inflate_fixed_tree.size());

						_state = state_read { std::move(state->stream), lt, std::nullopt, 0, 0 };
					} else if (type == COBRA_DEFLATE_DYNAMIC) {
						std::size_t hl = co_await state->stream.read_bits(5) + 257;
						std::size_t hd = co_await state->stream.read_bits(5) + 1;
						std::size_t hc = co_await state->stream.read_bits(4) + 4;

						std::array<std::size_t, 320> l;
						std::array<std::size_t, 19> lc;

						std::fill(l.begin(), l.end(), 0);
						std::fill(lc.begin(), lc.end(), 0);

						for (std::size_t i = 0; i < hc; i++) {
							lc[frobnication_table[i]] = co_await state->stream.read_bits(3);
						}

						inflate_ctree ct(lc.data(), lc.size());

						for (std::size_t i = 0; i < hl + hd;) {
							std::uint8_t v = co_await ct.read(state->stream);
							std::size_t n = co_await decode_code(state->stream, v);

							if (i + n > hl + hd) {
								throw compress_error::bad_trees;
							} else if (v == 16 && i == 0) {
								throw compress_error::bad_trees;
							} else if (v == 16) {
								v = l[i - 1];
							} else if (v == 17 || v == 18) {
								v = 0;
							}

							while (n-- > 0) {
								l[i++] = v;
							}
						}

						inflate_ltree lt(l.data(), hl);
						inflate_dtree dt(l.data() + hl, hd);

						_state = state_read { std::move(state->stream), lt, dt, 0, 0 };
					} else {
						throw compress_error::bad_block_type;
					}
				} else if (auto* state = std::get_if<state_write>(&_state)) {
					state->limit -= co_await base::write(state->stream, state->limit);

					if (state->limit == 0) {
						_state = state_init { bit_istream(std::move(state->stream)) };
					}
				} else if (auto* state = std::get_if<state_read>(&_state)) {
					if (state->size > 0) {
						state->size -= base::copy(state->dist, state->size);
					} else {
						std::uint16_t code = co_await state->lt.read(state->stream);

						if (code < 256) {
							char c = std::char_traits<char>::to_char_type(code);
							base::write(&c, 1);
						} else if (code == 256) {
							_state = state_init { std::move(state->stream) };
						} else {
							state->size = co_await decode_size(state->stream, code);
							code = state->dt ? co_await state->dt->read(state->stream) : co_await state->stream.read_bits(5);
							state->dist = co_await decode_dist(state->stream, code);
						}
					}
				}
			}
		}

		Stream end()&& {
			if (_final) {
				if (auto* state = std::get_if<state_init>(&_state)) {
					return std::move(state->stream).end();
				}
			}

			throw compress_error::not_finished;
		}
	};
}

#endif
