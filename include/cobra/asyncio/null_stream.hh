#ifndef COBRA_ASYNCIO_NULL_STREAM_HH
#define COBRA_ASYNCIO_NULL_STREAM_HH

#include "cobra/asyncio/stream.hh"

namespace cobra {
	template <class CharT, class Traits = std::char_traits<CharT>>
	class basic_null_ostream : public buffered_ostream_impl<basic_null_ostream<CharT, Traits>> {
	public:
		using typename buffered_ostream_impl<basic_null_ostream<CharT, Traits>>::char_type;

		task<std::size_t> write(const char_type* data, std::size_t size) {
			(void)data;
			co_return size;
		}

		task<void> flush() {
			co_return;
		}
	};

	using null_ostream = basic_null_ostream<char>;
} // namespace cobra

#endif
