#ifndef COBRA_COMPRESS_ERROR_HH
#define COBRA_COMPRESS_ERROR_HH

namespace cobra {
	enum class compress_error {
		not_finished,
		short_buffer,
		long_distance,
		bad_block_type,
		bad_len_check,
		bad_size_code,
		bad_dist_code,
		bad_huffman_code,
		bad_trees,
		tree_too_stupid,
		unsupported,
	};
} // namespace cobra

#endif
