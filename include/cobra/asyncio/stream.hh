#ifndef COBRA_ASYNCIO_STREAM_HH
#define COBRA_ASYNCIO_STREAM_HH

#include "cobra/asyncio/task.hh"

#include <string>

namespace cobra {
	template<class CharT, class Traits = std::char_traits<CharT>>
	class basic_stream {
	public:
		using char_type = CharT;
		using traits_type = Traits;
		using int_type = typename Traits::int_type;
		using pos_type = typename Traits::pos_type;
		using off_type = typename Traits::off_type;

		virtual ~basic_stream() {
		}
	};

	template<class CharT, class Traits = std::char_traits<CharT>>
	class basic_istream : public basic_stream<CharT, Traits> {
	public:
		using typename basic_stream<CharT, Traits>::char_type;
		using typename basic_stream<CharT, Traits>::traits_type;
		using typename basic_stream<CharT, Traits>::int_type;
		using typename basic_stream<CharT, Traits>::pos_type;
		using typename basic_stream<CharT, Traits>::off_type;

		virtual task<std::size_t> read(char_type* data, std::size_t size) = 0;
	};

	template<class CharT, class Traits = std::char_traits<CharT>>
	class basic_ostream : public basic_stream<CharT, Traits> {
	public:
		using typename basic_stream<CharT, Traits>::char_type;
		using typename basic_stream<CharT, Traits>::traits_type;
		using typename basic_stream<CharT, Traits>::int_type;
		using typename basic_stream<CharT, Traits>::pos_type;
		using typename basic_stream<CharT, Traits>::off_type;

		virtual task<std::size_t> write(const char_type* data, std::size_t size) = 0;
		virtual task<void> flush() = 0;

		virtual task<std::size_t> write_all(const char_type* data, std::size_t size) {
			std::size_t index = 0;
			std::size_t ret = 1;

			while (index < size && ret > 0) {
				ret = co_await write(data + index, size - index);
				index += ret;
			}

			co_return index;
		}
	};

	using stream = basic_stream<char>;
	using istream = basic_istream<char>;
	using ostream = basic_ostream<char>;
}

#endif
