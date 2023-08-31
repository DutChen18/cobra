#ifndef COBRA_COMPRESS_DEFLATE_HH
#define COBRA_COMPRESS_DEFLATE_HH

#include "cobra/compress/bit_stream.hh"
#include "cobra/compress/lz.hh"
#include "cobra/compress/stream_ringbuffer.hh"
#include "cobra/compress/tree.hh"

#include <bit>

#define COBRA_DEFLATE_NONE 0
#define COBRA_DEFLATE_FIXED 1
#define COBRA_DEFLATE_DYNAMIC 2

namespace cobra {
	using inflate_ltree = inflate_tree<std::uint16_t, 288, 15>;
	using inflate_dtree = inflate_tree<std::uint16_t, 32, 15>;
	using inflate_ctree = inflate_tree<std::uint8_t, 19, 7>;
	using deflate_ltree = deflate_tree<std::uint16_t, 288, 15>;
	using deflate_dtree = deflate_tree<std::uint16_t, 32, 15>;
	using deflate_ctree = deflate_tree<std::uint8_t, 19, 7>;

	constexpr std::array<std::size_t, 19> frobnication_table{
		16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15,
	};

	constexpr std::array<std::size_t, 288> fixed_tree{
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
		9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
		9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
		9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
		9, 9, 9, 9, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8,
	};

	enum class deflate_mode {
		raw,
		zlib,
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
		deflate_mode _mode;
		bool _read_header = false;

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
		inflate_istream(Stream&& stream, deflate_mode mode = deflate_mode::raw)
			: base(32768), _state(state_init{bit_istream(std::move(stream))}), _mode(mode) {}

		task<void> fill_ringbuf() {
			while (!base::full()) {
				if (auto* state = std::get_if<state_init>(&_state)) {
					if (_final) {
						co_return;
					}

					if (!_read_header) {
						if (_mode == deflate_mode::zlib) {
							throw compress_error::unsupported;
						}

						_read_header = true;
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

						_state = state_write{std::move(stream), len};
					} else if (type == COBRA_DEFLATE_FIXED) {
						inflate_ltree lt(fixed_tree.data(), fixed_tree.size());

						_state = state_read{std::move(state->stream), lt, std::nullopt, 0, 0};
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

						inflate_ctree ct(lc.data(), 19);

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
							} else {
							}

							while (n-- > 0) {
								l[i++] = v;
							}
						}

						inflate_ltree lt(l.data(), hl);
						inflate_dtree dt(l.data() + hl, hd);

						_state = state_read{std::move(state->stream), lt, dt, 0, 0};
					} else {
						throw compress_error::bad_block_type;
					}
				} else if (auto* state = std::get_if<state_write>(&_state)) {
					state->limit -= co_await base::write(state->stream, state->limit);

					if (state->limit == 0) {
						_state = state_init{bit_istream(std::move(state->stream))};
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
							_state = state_init{std::move(state->stream)};
						} else {
							state->size = co_await decode_size(state->stream, code);
							code = state->dt ? co_await state->dt->read(state->stream)
											 : reverse(co_await state->stream.read_bits(5), 5);
							state->dist = co_await decode_dist(state->stream, code);
						}
					}
				}
			}
		}

		Stream end() && {
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
		deflate_mode _mode;
		bool _wrote_header = false;

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
			return {static_cast<std::uint16_t>(extra_bits * stride + start_offset + code), extra_bits, value};
		}

		static token encode_code(std::uint16_t code, std::size_t* value, std::size_t max) {
			std::size_t count = std::min(*value, max);
			*value -= count;

			if (code == 16) {
				return {code, 2, static_cast<std::uint16_t>(count - 3)};
			} else if (code == 17) {
				return {code, 3, static_cast<std::uint16_t>(count - 3)};
			} else if (code == 18) {
				return {code, 7, static_cast<std::uint16_t>(count - 11)};
			} else {
				return {code, 0, 0};
			}
		}

		static token encode_size(std::uint16_t size) {
			if (size == 258) {
				return {285, 0, 0};
			} else if (size < 7) {
				return {static_cast<std::uint16_t>(size + 257 - 3), 0, 0};
			} else {
				return encode(261, 4, size - 7);
			}
		}

		static token encode_dist(std::uint16_t dist) {
			if (dist < 3) {
				return {static_cast<std::uint16_t>(dist - 1), 0, 0};
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

		task<void> write_block(const deflate_ltree* lt, const deflate_dtree* dt) {
			for (const lz_command& command : _commands) {
				if (command.is_literal()) {
					co_await lt->write(_stream, command.ch());
				} else {
					assert(command.length() >= 3);
					assert(command.dist() >= 1);

					token size_token = encode_size(command.length());
					co_await lt->write(_stream, size_token.code);
					co_await _stream.write_bits(size_token.value, size_token.extra);
					token dist_token = encode_dist(command.dist());

					if (dt) {
						co_await dt->write(_stream, dist_token.code);
					} else {
						co_await _stream.write_bits(reverse(dist_token.code, 5), 5);
					}

					co_await _stream.write_bits(dist_token.value, dist_token.extra);
				}
			}

			co_await lt->write(_stream, 256);

			reset();
		}

		task<void> flush_block(bool end) {
			if (!_wrote_header) {
				if (_mode == deflate_mode::zlib) {
					co_await _stream.write_bits(0x78, 8);
					co_await _stream.write_bits(0x9C, 8);
				}

				_wrote_header = true;
			}

			if (_commands.size() >= 20) {
				deflate_ltree lt = deflate_ltree::plant(_size_weight.data(), 288);
				deflate_dtree dt = deflate_dtree::plant(_dist_weight.data(), 32);
				std::array<std::size_t, 320> l;
				std::array<std::size_t, 19> code_weight;
				std::array<token, 320> code_code;
				std::size_t code_size = 0;
				std::size_t hl = lt.get_size(l.data(), nullptr);
				std::size_t hd = dt.get_size(l.data() + hl, nullptr);

				for (std::size_t i = 0, n, m; i < hl + hd; i += m) {
					for (n = 1; i + n < hl + hd && l[i] == l[i + n]; n++)
						;

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

				co_await write_block(&lt, &dt);
			} else {
				deflate_ltree lt(fixed_tree.data(), fixed_tree.size());

				co_await _stream.write_bits(end ? 1 : 0, 1);
				co_await _stream.write_bits(1, 2);
				co_await write_block(&lt, nullptr);
			}
		}

	public:
		deflate_ostream_impl(Stream&& stream, deflate_mode mode) : _stream(std::move(stream)), _mode(mode) {
			reset();
		}

		deflate_ostream_impl(deflate_ostream_impl&& other)
			: _stream(std::move(other._stream)), _commands(std::move(other._commands)),
			  _size_weight(std::move(other._size_weight)), _dist_weight(std::move(other._dist_weight)),
			  _mode(std::move(other._mode)), _wrote_header(std::move(other._wrote_header)) {}

		~deflate_ostream_impl() {
			assert(_commands.empty());
		}

		deflate_ostream_impl& operator=(deflate_ostream_impl other) {
			std::swap(_stream, other._stream);
			std::swap(_commands, other._commands);
			std::swap(_size_weight, other._size_weight);
			std::swap(_dist_weight, other._dist_weight);
			std::swap(_mode, other._mode);
			std::swap(_wrote_header, other._wrote_header);
			return *this;
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

		task<Stream> end() && {
			co_await flush_block(true);
			co_return co_await std::move(_stream).end();
		}
	};

	// ODOT: the inheritence here is super janky
	template <AsyncOutputStream Stream>
	class deflate_ostream : public buffered_ostream_impl<deflate_ostream<Stream>> {
		using base = buffered_ostream_impl<deflate_ostream<Stream>>;

		lz_ostream<deflate_ostream_impl<Stream>> _inner;
		deflate_mode _mode;
		std::uint32_t adler32_a = 1;
		std::uint32_t adler32_b = 0;

		constexpr static std::size_t window_size = 32768;

	public:
		using typename base::char_type;

		deflate_ostream(Stream&& stream, deflate_mode mode = deflate_mode::raw)
			: _inner(deflate_ostream_impl(std::move(stream), mode), window_size), _mode(mode) {}

		task<std::size_t> write(const char_type* data, std::size_t size) {
			for (std::size_t i = 0; i < size; i++) {
				adler32_a = (adler32_a + static_cast<std::uint8_t>(data[i])) % 65521;
				adler32_b = (adler32_b + adler32_a) % 65521;
			}

			return _inner.write(data, size);
		}

		task<void> flush() {
			return _inner.flush();
		}

		task<Stream> end() && {
			auto tmp2 = co_await std::move(_inner).end();
			Stream tmp = co_await std::move(tmp2).end();

			if (_mode == deflate_mode::zlib) {
				co_await cobra::write_u32_be(tmp, adler32_b << 16 | adler32_a);
			}

			co_return std::move(tmp);
		}
	};
} // namespace cobra

#endif
