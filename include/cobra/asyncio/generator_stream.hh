#ifndef COBRA_ASYNCIO_GENERATOR_STREAM_HH
#define COBRA_ASYNCIO_GENERATOR_STREAM_HH

#include "cobra/asyncio/generator.hh"
#include "cobra/asyncio/stream.hh"

#include <string>

namespace cobra {
	template <class Generator>
	class generator_stream
		: public basic_buffered_istream_impl<generator_stream<Generator>, typename Generator::result_type::value_type,
											 typename Generator::result_type::traits_type> {
	public:
		using base =
			basic_buffered_istream_impl<generator_stream<Generator>, typename Generator::result_type::value_type,
										typename Generator::result_type::traits_type>;
		using typename base::char_type;
		using typename base::traits_type;

		Generator _generator;
		std::basic_string<char_type, traits_type> _buffer;
		std::size_t _index = 0;

	public:
		generator_stream(Generator&& generator) : _generator(std::move(generator)) {}

		task<std::pair<const char_type*, std::size_t>> fill_buf() {
			generator_iterator begin = _generator.begin();
			generator_iterator end = _generator.end();

			while (_index >= _buffer.size()) {
				if (begin == end) {
					co_return {nullptr, 0};
				}

				_buffer = *begin;
				_index = 0;
				++begin;
			}

			co_return {_buffer.data() + _index, _buffer.size() - _index};
		}

		void consume(std::size_t size) {
			_index += size;
		}
	};
} // namespace cobra

#endif
