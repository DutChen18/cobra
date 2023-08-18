#ifndef COBRA_COMPRESS_TREE_HH
#define COBRA_COMPRESS_TREE_HH

#include "cobra/asyncio/stream.hh"
#include "cobra/compress/bit_stream.hh"
#include "cobra/compress/error.hh"

#include <array>
#include <numeric>
#include <vector>
#include <algorithm>

namespace cobra {
	template <class T, std::size_t Size, std::size_t Bits>
	class inflate_tree {
		std::array<T, Size> _data;
		std::array<T, Bits + 1> _count;

	public:
		// TODO: sanitize
		inflate_tree(const std::size_t* size, std::size_t n) {
			std::array<T, Bits + 1> next;

			std::fill(_count.begin(), _count.end(), 0);

			for (T i = 0; i < n; i++) {
				if (size[i] != 0) {
					_count[size[i]] += 1;
				}
			}

			std::partial_sum(_count.begin(), std::prev(_count.end()), std::next(next.begin()));

			for (T i = 0; i < n; i++) {
				if (size[i] != 0) {
					if (next[size[i]] >= Size) {
						throw compress_error::tree_too_stupid;
					}

					_data[next[size[i]]++] = i;
				}
			}
		}

		template <AsyncInputStream Stream>
		task<T> read(bit_istream<Stream>& stream) const {
			T offset = 0;
			T value = 0;

			for (std::size_t i = 0; i <= Bits; i++) {
				if (value < _count[i]) {
					co_return _data[value + offset];
				}

				offset += _count[i];
				value -= _count[i];
				value <<= 1;
				value |= co_await stream.read_bits(1);
			}

			throw compress_error::bad_huffman_code;
		}
	};

	constexpr std::uint64_t reverse(std::uint64_t value, std::size_t bits) {
		value = ((value & UINT64_C(0xAAAAAAAAAAAAAAAA)) >> 1) | ((value & UINT64_C(0x5555555555555555)) << 1);
		value = ((value & UINT64_C(0xCCCCCCCCCCCCCCCC)) >> 2) | ((value & UINT64_C(0x3333333333333333)) << 2);
		value = ((value & UINT64_C(0xF0F0F0F0F0F0F0F0)) >> 4) | ((value & UINT64_C(0x0F0F0F0F0F0F0F0F)) << 4);
		value = ((value & UINT64_C(0xFF00FF00FF00FF00)) >> 8) | ((value & UINT64_C(0x00FF00FF00FF00FF)) << 8);
		value = ((value & UINT64_C(0xFFFF0000FFFF0000)) >> 16) | ((value & UINT64_C(0x0000FFFF0000FFFF)) << 16);
		value = ((value & UINT64_C(0xFFFFFFFF00000000)) >> 32) | ((value & UINT64_C(0x00000000FFFFFFFF)) << 32);
		return value >> (64 - bits);
	}

	template <class T, std::size_t Size, std::size_t Bits>
	class deflate_tree {
		std::array<std::size_t, Size> _size;
		std::array<T, Size> _data;

		struct node {
			std::size_t weight;
			T index;
			T code;
		};

		struct list {
			std::array<node, Size * 2> nodes;
			std::size_t size;
		};

		static void plant_find_size(const std::array<list, Bits + 1>& lists, std::array<std::size_t, Size>& size, std::size_t i, std::size_t j) {
			if (lists[i].nodes[j].index < Size) {
				plant_find_size(lists, size, i - 1, (std::size_t) lists[i].nodes[j].index * 2);
				plant_find_size(lists, size, i - 1, (std::size_t) lists[i].nodes[j].index * 2 + 1);
			}

			if (lists[i].nodes[j].code < Size) {
				size[lists[i].nodes[j].code] += 1;
			}
		}

	public:
		// TODO: sanitize
		deflate_tree(const std::size_t* size, std::size_t n) {
			std::array<T, Bits + 1> count;
			std::array<T, Bits + 1> next;

			std::fill(count.begin(), count.end(), 0);
			std::copy(size, size + n, _size.data());

			for (T i = 0; i < n; i++) {
				if (size[i] != 0) {
					count[size[i]] += 1;
				}
			}

			for (std::size_t i = 1; i < Bits; i++) {
				next[i] = (next[i - 1] + count[i - 1]) << 1;
			}

			for (T i = 0; i < n; i++) {
				if (size[i] != 0) {
					if (next[size[i]] >= Size) {
						throw compress_error::tree_too_stupid;
					}

					_data[i] = reverse(next[size[i]]++, size[i]);
				}
			}
		}

		std::size_t get_size(std::size_t* size, const std::size_t* frobnication_table) {
			std::size_t max;

			for (std::size_t i = 0; i < Size; i++) {
				std::size_t j = frobnication_table ? frobnication_table[i] : i;

				if (size) {
					size[j] = _size[j];
				}

				if (_size[j] != 0) {
					max = i + 1;
				}
			}

			return max;
		}

		template <AsyncOutputStream Stream>
		task<void> write(bit_ostream<Stream>& stream, T value) {
			co_await stream.write_bits(_data[value], _size[value]);
		}

		static deflate_tree plant(const std::size_t* weight, std::size_t n) {
			std::array<list, Bits + 1> lists;
			std::array<std::size_t, Size> size;

			lists[0].size = 0;

			for (T i = 0; i < n; i++) {
				if (weight[i] != 0) {
					lists[0].nodes[lists[0].size].weight = weight[i];
					lists[0].nodes[lists[0].size].index = Size;
					lists[0].nodes[lists[0].size].code = i;
					lists[0].size += 1;
				}
			}

			std::sort(lists[0].nodes.data(), lists[0].nodes.data() + lists[0].size, [](const node& a, const node& b) {
				return a.weight < b.weight;
			});

			for (std::size_t i = 1; i <= Bits; i++) {
				std::size_t pair_index = 0;
				std::size_t leaf_index = 0;

				if (i == Bits) {
					leaf_index = lists[0].size;
				}

				while (leaf_index < lists[0].size || pair_index < lists[i - 1].size / 2) {
					std::size_t leaf_weight = std::numeric_limits<std::size_t>::max();
					std::size_t pair_weight = std::numeric_limits<std::size_t>::max();

					if (leaf_index < lists[0].size) {
						leaf_weight = lists[0].nodes[leaf_index].weight;
					}

					if (pair_index < lists[i - 1].size / 2) {
						pair_weight = lists[i - 1].nodes[pair_index * 2].weight + lists[i - 1].nodes[pair_index * 2 + 1].weight;
					}

					if (leaf_weight < pair_weight) {
						lists[i].nodes[lists[i].size].weight = leaf_weight;
						lists[i].nodes[lists[i].size].index = Size;
						lists[i].nodes[lists[i].size].code = lists[0].nodes[leaf_index].code;
						leaf_index += 1;
					} else {
						lists[i].nodes[lists[i].size].weight = pair_weight;
						lists[i].nodes[lists[i].size].index = pair_index;
						lists[i].nodes[lists[i].size].code = Size;
						pair_index += 1;
					}

					lists[i].size += 1;
				}
			}

			std::fill(size.data(), size.data() + n, 0);

			for (std::size_t i = 0; i < lists[Bits].size; i++) {
				plant_find_size(lists, size, Bits, i);
			}

			if (lists[Bits - 1].size == 0) {
				if (lists[0].size == 0) {
					size[0] += 1;
				} else {
					size[lists[0].nodes[0].code] += 1;
				}
			}

			return deflate_tree(size.data(), n);
		}
	};
}

#endif
