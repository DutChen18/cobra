#ifndef COBRA_COMPRESS_DEFLATE_HH
#define COBRA_COMPRESS_DEFLATE_HH

#include "cobra/compress/bit_stream.hh"
#include "cobra/compress/stream_ringbuffer.hh"
#include "cobra/compress/tree.hh"
#include "cobra/compress/lz.hh"

#include <bit>

#define COBRA_DEFLATE_NONE 0
#define COBRA_DEFLATE_FIXED 1
#define COBRA_DEFLATE_DYNAMIC 2

namespace cobra {
	using inflate_ltree = inflate_tree<std::uint16_t, 288, 15>;
	using inflate_dtree = inflate_tree<std::uint8_t, 32, 15>;
	using inflate_ctree = inflate_tree<std::uint8_t, 19, 7>;
	using deflate_ltree = deflate_tree<std::uint16_t, 288, 15>;
	using deflate_dtree = deflate_tree<std::uint8_t, 32, 15>;
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

						eprintln("count {} {} {}", hl, hd, hc);

						std::array<std::size_t, 320> l;
						std::array<std::size_t, 19> lc;

						std::fill(l.begin(), l.end(), 0);
						std::fill(lc.begin(), lc.end(), 0);

						for (std::size_t i = 0; i < hc; i++) {
							lc[frobnication_table[i]] = co_await state->stream.read_bits(3);

							if (lc[frobnication_table[i]] != 0) {
								eprintln("code {} {}", frobnication_table[i], lc[frobnication_table[i]]);
							}
						}

						inflate_ctree ct(lc.data(), hc);

						for (std::size_t i = 0; i < hl + hd;) {
							std::uint8_t v = co_await ct.read(state->stream);
							std::size_t n = co_await decode_code(state->stream, v);

							if (i + n > hl + hd) {
								throw compress_error::bad_trees;
							} else if (v == 16 && i == 0) {
								throw compress_error::bad_trees;
							} else if (v == 16) {
								eprintln("repeat {}", n);
								v = l[i - 1];
							} else if (v == 17 || v == 18) {
								eprintln("zeros {}", n);
								v = 0;
							} else {
								eprintln("len {}", v);
							}

							while (n-- > 0) {
								l[i++] = v;
							}
						}

						eprintln("done");

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

	template <AsyncOutputStream Stream>
	class deflate_ostream_impl {
		bit_ostream<Stream> _stream;
		std::vector<lz_command> _commands;
		std::array<std::size_t, 288> _size_weight;
		std::array<std::size_t, 32> _dist_weight;

		struct token {
			std::uint16_t code;
			std::uint16_t extra;
			std::uint16_t value;
		};

		static token encode(std::uint16_t code, std::uint16_t stride, std::uint16_t value) {
			std::uint16_t extra_bits = 15 - std::countl_zero(static_cast<std::uint16_t>(value / stride + 1));
			std::uint16_t block_offset = (stride << extra_bits) - stride;
			std::uint16_t start_offset = (value - block_offset) >> extra_bits;
			value -= block_offset + (start_offset << extra_bits);
			return { static_cast<std::uint16_t>(extra_bits * stride + start_offset + code), extra_bits, value };
		}

		static token encode_code(std::uint16_t code, std::size_t* value, std::size_t max) {
			std::size_t count = std::min(*value, max);
			*value -= count;

			if (code == 16) {
				return { code, 2, static_cast<std::uint16_t>(count - 3) };
			} else if (code == 17) {
				return { code, 3, static_cast<std::uint16_t>(count - 3) };
			} else if (code == 18) {
				return { code, 7, static_cast<std::uint16_t>(count - 11) };
			} else {
				return { code, 0, 0 };
			}
		}

		static token encode_size(std::uint16_t size) {
			if (size == 258) {
				return { 285, 0, 0 };
			} else if (size < 7) {
				return { static_cast<std::uint16_t>(size + 257 - 3), 0, 0 };
			} else {
				return encode(261, 4, size - 7);
			}
		}

		static token encode_dist(std::uint16_t dist) {
			if (dist < 3) {
				return { static_cast<std::uint16_t>(dist - 1), 0, 0 };
			} else {
				return encode(2, 2, dist - 3);
			}
		}

		void reset() {
			std::fill(_size_weight.begin(), _size_weight.end(), 0);
			std::fill(_dist_weight.begin(), _dist_weight.end(), 0);
			_commands.clear();
			_size_weight[256] += 1;
		}

		task<void> flush_block(bool end) {
			deflate_ltree lt = deflate_ltree::plant(_size_weight.data(), 288);
			deflate_dtree dt = deflate_dtree::plant(_dist_weight.data(), 32);
			std::array<std::size_t, 320> l;
			std::array<std::size_t, 19> code_weight;
			std::array<token, 320> code_code;
			std::size_t code_size = 0;
			std::size_t hl = lt.get_size(l.data(), nullptr);
			std::size_t hd = dt.get_size(l.data() + hl, nullptr);

			for (std::size_t i = 0, n, m; i < hl + hd;i += m) {
				for (n = 1; i + n < hl + hd && l[i] == l[i + n]; n++);

				m = n;

				if (l[i] == 0) {
					while (n >= 11) {
						code_code[code_size++] = encode_code(18, &n, 138);
					}

					while (n >= 3) {
						code_code[code_size++] = encode_code(17, &n, 10);
					}
				} else {
					code_code[code_size++] = encode_code(l[i], &n, 1);
					
					while (n >= 3) {
						code_code[code_size++] = encode_code(16, &n, 6);
					}
				}

				while (n >= 1) {
					code_code[code_size++] = encode_code(l[i], &n, 1);
				}
			}

			std::fill(code_weight.begin(), code_weight.end(), 0);

			for (std::size_t i = 0; i < code_size; i++) {
				code_weight[code_code[i].code] += 1;
			}

			deflate_ctree ct = deflate_ctree::plant(code_weight.data(), 19);
			std::array<std::size_t, 19> lc;
			std::size_t hc = ct.get_size(lc.data(), frobnication_table.data());

			assert(hl >= 257 && "bad hl");
			assert(hd >= 1 && "bad hd");
			assert(hc >= 4 && "bad hc");

			co_await _stream.write_bits(end ? 1 : 0, 1);
			co_await _stream.write_bits(2, 2);
			co_await _stream.write_bits(hl - 257, 5);
			co_await _stream.write_bits(hd - 1, 5);
			co_await _stream.write_bits(hc - 4, 4);

			for (std::size_t i = 0; i < hc; i++) {
				assert(lc[i] < 8 && "code length length too lengthy");
				co_await _stream.write_bits(lc[i], 3);
			}

			for (std::size_t i = 0; i < code_size; i++) {
				co_await ct.write(_stream, code_code[i].code);
				co_await _stream.write_bits(code_code[i].value, code_code[i].extra);
			}

			for (const lz_command& command : _commands) {
				if (command.is_literal()) {
					co_await lt.write(_stream, command.ch());
				} else {
					token size_token = encode_size(command.length());
					co_await lt.write(_stream, size_token.code);
					co_await _stream.write_bits(size_token.value, size_token.extra);
					token dist_token = encode_dist(command.dist());
					co_await dt.write(_stream, dist_token.code);
					co_await _stream.write_bits(dist_token.value, dist_token.extra);
				}
			}

			co_await lt.write(_stream, 256);

			reset();
		}

	public:
		deflate_ostream_impl(Stream&& stream) : _stream(std::move(stream)) {
			reset();
		}

		task<void> write(const lz_command& command) {
			_commands.push_back(command);

			if (command.is_literal()) {
				_size_weight[command.ch()] += 1;
			} else {
				_size_weight[encode_size(command.length()).code] += 1;
				_dist_weight[encode_dist(command.dist()).code] += 1;
			}

			if (_commands.size() >= 32768) {
				co_await flush_block(false);
			}
		}

		task<void> flush() {
			if (!_commands.empty()) {
				co_await flush_block(false);
			}

			co_await flush_block(false);
			co_await _stream.flush();
		}

		task<Stream> end()&& {
			co_await flush_block(true);
			co_return co_await std::move(_stream).end();
		}
	};

	template <AsyncOutputStream Stream>
	class deflate_ostream : public lz_ostream<deflate_ostream_impl<Stream>> {
		using base = lz_ostream<deflate_ostream_impl<Stream>>;

	public:
		deflate_ostream(Stream&& stream) : base(std::move(stream), 32768) {
		}

		task<Stream> end()&& {
			co_return co_await (co_await std::move(*static_cast<base*>(this)).end()).end();
		}
	};
}

#endif
