#ifndef COBRA_TEST_STRINGSTREAM
#define COBRA_TEST_STRINGSTREAM

#include "cobra/asyncio/stream.hh"
#include <streambuf>
#include <string>
#include <memory>
#include <utility>

#include <iostream>

namespace test {

	template <class CharT, class Traits = std::char_traits<CharT>, class Alloc = std::allocator<CharT>>
	class basic_istringstream : public cobra::basic_buffered_istream_impl<basic_istringstream<CharT, Traits, Alloc>, CharT, Traits> {
	public:
		using base_type = cobra::basic_buffered_istream<CharT, Traits>;
		using char_type = CharT;
		using traits_type = Traits;
		using int_type = typename Traits::int_type;
		using pos_type = typename Traits::pos_type;
		using off_type = typename Traits::off_type;
		using allocator_type =  Alloc;
		using string_type = std::basic_string<char_type, traits_type, allocator_type>;
		using size_type = typename string_type::size_type;

	private:
		string_type _str;
		size_type _offset;

	public:
		basic_istringstream(const basic_istringstream& other) = delete;
		constexpr basic_istringstream(string_type str, size_type offset = 0) noexcept : _str(std::move(str)), _offset(offset) {}
		constexpr basic_istringstream(basic_istringstream&& other) noexcept : _str(std::move(other._str)), _offset(std::exchange(other._offset, 0)) {}

		cobra::task<size_type> read(char_type* dst, size_type count) {
			size_type nread = std::min(remaining(), count);
			std::uninitialized_copy_n(_str.cbegin() + _offset, nread, dst);
			_offset += nread;
			co_return nread;
		}

		cobra::task<std::optional<char_type>> peek() {
			if (remaining() > 0) {
				co_return _str[_offset];
			}
			co_return std::nullopt;
		};

		inline size_type remaining() const { return _str.size() - _offset; }
	};

	using istringstream = basic_istringstream<char>;
}

#endif
