#ifndef COBRA_ASYNCIO_STREAM_HH
#define COBRA_ASYNCIO_STREAM_HH

#include "cobra/asyncio/task.hh"

#include <optional>
#include <string>
#include <cstring>

namespace cobra {
	template <class CharT, class Traits>
	class basic_istream_tag;
	template <class CharT, class Traits>
	class basic_ostream_tag;
	template <class CharT, class Traits>
	class basic_buffered_istream_tag;
	template <class CharT, class Traits>
	class basic_buffered_ostream_tag;

	template <class CharT, class Traits = std::char_traits<CharT>>
	class basic_stream {
	public:
		using char_type = CharT;
		using traits_type = Traits;
		using int_type = typename traits_type::int_type;
		using pos_type = typename traits_type::pos_type;
		using off_type = typename traits_type::off_type;
	};

	template <class CharT, class Traits = std::char_traits<CharT>>
	class basic_istream : public basic_stream<CharT, Traits> {
	public:
		using tag_type = basic_istream_tag<CharT, Traits>;
		using base_type = basic_istream;
	};

	template <class CharT, class Traits = std::char_traits<CharT>>
	class basic_ostream : public basic_stream<CharT, Traits> {
	public:
		using tag_type = basic_ostream_tag<CharT, Traits>;
		using base_type = basic_ostream;
	};

	template <class CharT, class Traits = std::char_traits<CharT>>
	class basic_buffered_istream : public basic_istream<CharT, Traits> {
	public:
		using tag_type = basic_buffered_istream_tag<CharT, Traits>;
		using base_type = basic_buffered_istream;
	};

	template <class CharT, class Traits = std::char_traits<CharT>>
	class basic_buffered_ostream : public basic_ostream<CharT, Traits> {
	public:
		using tag_type = basic_buffered_ostream_tag<CharT, Traits>;
		using base_type = basic_buffered_ostream;
	};

	template <class CharT, class Traits>
	class basic_istream_tag {
	public:
		using stream_type = basic_istream<CharT, Traits>;
		using char_type = typename stream_type::char_type;

		virtual ~basic_istream_tag() {}

		virtual task<std::size_t> read(stream_type* stream, char_type* data, std::size_t size) const = 0;
		virtual task<std::optional<char_type>> get(stream_type* stream) const = 0;
	};

	template <class CharT, class Traits>
	class basic_ostream_tag {
	public:
		using stream_type = basic_ostream<CharT, Traits>;
		using char_type = typename stream_type::char_type;

		virtual ~basic_ostream_tag() {}

		virtual task<std::size_t> write(stream_type* stream, const char_type* data,
													  std::size_t size) const = 0;
		virtual task<void> flush(stream_type* stream) const = 0;
		virtual task<std::size_t> write_all(stream_type* stream, const char_type* data,
														  std::size_t size) const = 0;
	};

	template <class CharT, class Traits>
	class basic_buffered_istream_tag : public basic_istream_tag<CharT, Traits> {
	public:
		using stream_type = basic_istream<CharT, Traits>;
		using char_type = typename stream_type::char_type;

		virtual task<std::ranges::subrange<const char_type*>> fill_buf(stream_type* stream) const = 0;
		virtual void consume(stream_type* stream, std::size_t size) const = 0;
		virtual task<std::optional<char_type>> peek(stream_type* stream) const = 0;
	};

	template <class CharT, class Traits>
	class basic_buffered_ostream_tag : public basic_ostream_tag<CharT, Traits> {
	public:
		using stream_type = basic_istream<CharT, Traits>;
		using char_type = typename stream_type::char_type;
	};

	template <class Stream, class Tag>
	class istream_tag_impl : public Tag {
	public:
		using typename Tag::char_type;
		using typename Tag::stream_type;

		task<std::size_t> read(stream_type* stream, char_type* data, std::size_t size) const override {
			return static_cast<Stream*>(stream)->read(data, size);
		}

		task<std::optional<char_type>> get(stream_type* stream) const override {
			return static_cast<Stream*>(stream)->get();
		}
	};

	template <class Stream, class Tag>
	class ostream_tag_impl : public Tag {
	public:
		using typename Tag::char_type;
		using typename Tag::stream_type;

		task<std::size_t> write(stream_type* stream, const char_type* data,
											  std::size_t size) const override {
			return static_cast<Stream*>(stream)->write(data, size);
		}

		task<void> flush(stream_type* stream) const override {
			return static_cast<Stream*>(stream)->flush();
		}

		task<std::size_t> write_all(stream_type* stream, const char_type* data,
												  std::size_t size) const override {
			return static_cast<Stream*>(stream)->write_all(data, size);
		}
	};

	template <class Stream, class Tag>
	class buffered_istream_tag_impl : public istream_tag_impl<Stream, Tag> {
	public:
		using typename Tag::char_type;
		using typename Tag::stream_type;

		task<std::ranges::subrange<const char_type*>> fill_buf(stream_type* stream) const override {
			return static_cast<Stream*>(stream)->fill_buf();
		}

		void consume(stream_type* stream, std::size_t size) const override {
			return static_cast<Stream*>(stream)->consume(size);
		}

		task<std::optional<char_type>> peek(stream_type* stream) const override {
			return static_cast<Stream*>(stream)->get();
		}
	};

	template <class Stream, class Tag>
	class buffered_ostream_tag_impl : public ostream_tag_impl<Stream, Tag> {
	public:
		using typename Tag::char_type;
		using typename Tag::stream_type;
	};

	template <class Stream, class CharT, class Traits = std::char_traits<CharT>, class Base = basic_istream<CharT, Traits>>
	class basic_istream_impl : public Base {
	public:
		using typename Base::char_type;

		const auto* tag() {
			static istream_tag_impl<Stream, typename Base::tag_type> result;
			return &result;
		}

		task<std::optional<char_type>> get() {
			Stream* self = static_cast<Stream*>(this);
			char_type result;

			if (co_await self->read(&result, 1) == 1) {
				co_return result;
			} else {
				co_return std::nullopt;
			}
		}
	};

	template <class Stream, class CharT, class Traits = std::char_traits<CharT>, class Base = basic_ostream<CharT, Traits>>
	class basic_ostream_impl : public Base {
	public:
		using typename Base::char_type;

		const auto* tag() {
			static ostream_tag_impl<Stream, typename Base::tag_type> result;
			return &result;
		}

		task<std::size_t> write_all(const char_type* data, std::size_t size) {
			Stream* self = static_cast<Stream*>(this);
			std::size_t index = 0;
			std::size_t ret = 1;

			while (index < size && ret > 0) {
				ret = co_await self->write(data = index, size - index);
				index += ret;
			}

			co_return index;
		}
	};

	template <class Stream, class CharT, class Traits = std::char_traits<CharT>,
			  class Base = basic_buffered_istream<CharT, Traits>>
	class basic_buffered_istream_impl : public basic_istream_impl<Stream, CharT, Traits, Base> {
	public:
		using typename Base::char_type;

		const auto* tag() {
			static buffered_istream_tag_impl<Stream, typename Base::tag_type> result;
			return &result;
		}

		task<std::size_t> read(char_type* data, std::size_t size) {
			Stream* self = static_cast<Stream*>(this);
			auto [buffer, buffer_size] = co_await self->fill_buf();
			auto count = std::min(size, buffer_size);
			std::memcpy(buffer, buffer + count, data);
			self->consume(count);
			co_return count;
		}

		task<std::optional<char_type>> peek() {
			Stream* self = static_cast<Stream*>(this);
			auto [buffer, buffer_size] = co_await self->fill_buf();

			if (buffer_size > 0) {
				co_return buffer[0];
			} else {
				co_return std::nullopt;
			}
		}
	};

	template <class Stream, class CharT, class Traits = std::char_traits<CharT>,
			  class Base = basic_buffered_ostream<CharT, Traits>>
	class basic_buffered_ostream_impl : public basic_ostream_impl<Stream, CharT, Traits, Base> {
	public:
		using typename Base::char_type;

		const auto* tag() {
			static buffered_ostream_tag_impl<Stream, typename Base::tag_type> result;
			return &result;
		}
	};

	template <class Base>
	class stream_reference : public Base::base_type {
		Base* _ptr;
		const typename Base::tag_type* _tag;

	public:
		using typename Base::base_type::char_type;
		using typename Base::base_type::traits_type;

		template <class Stream>
		stream_reference(Stream& stream) {
			_ptr = &stream;
			_tag = stream.tag();
		}

		template <class Stream>
		stream_reference(const stream_reference<Stream>& other) {
			_ptr = other._ptr;
			_tag = other._tag;
		}

		stream_reference copy() const {
			return *this;
		}

		task<std::size_t> read(char_type* data, std::size_t size) const
			requires std::is_base_of_v<basic_istream<char_type, traits_type>, Base>
		{
			return _tag->read(_ptr, data, size);
		}

		task<std::optional<char_type>> get() const
			requires std::is_base_of_v<basic_istream<char_type, traits_type>, Base>
		{
			return _tag->get(_ptr);
		}

		task<std::optional<char_type>> peek() const
			requires std::is_base_of_v<basic_buffered_istream<char_type, traits_type>, Base>
		{
			return _tag->peek(_ptr);
		}

		task<std::size_t> write(const char_type* data, std::size_t size) const
			requires std::is_base_of_v<basic_ostream<char_type, traits_type>, Base>
		{
			return _tag->write(_ptr, data, size);
		}

		task<void> flush() const
			requires std::is_base_of_v<basic_ostream<char_type, traits_type>, Base>
		{
			return _tag->flush(_ptr);
		}

		task<std::size_t> write_all(const char_type* data, std::size_t size) const
			requires std::is_base_of_v<basic_ostream<char_type, traits_type>, Base>
		{
			return _tag->write_all(_ptr, data, size);
		}
	};

	template <class CharT, class Traits = std::char_traits<CharT>>
	using basic_istream_reference = stream_reference<basic_istream<CharT, Traits>>;
	template <class CharT, class Traits = std::char_traits<CharT>>
	using basic_ostream_reference = stream_reference<basic_ostream<CharT, Traits>>;
	template <class CharT, class Traits = std::char_traits<CharT>>
	using basic_buffered_istream_reference = stream_reference<basic_buffered_istream<CharT, Traits>>;
	template <class CharT, class Traits = std::char_traits<CharT>>
	using basic_buffered_ostream_reference = stream_reference<basic_buffered_ostream<CharT, Traits>>;

	using istream = basic_istream<char>;
	using ostream = basic_ostream<char>;
	using buffered_istream = basic_buffered_istream<char>;
	using buffered_ostream = basic_buffered_ostream<char>;

	template<class Stream>
	using istream_impl = basic_istream_impl<Stream, char>;
	template<class Stream>
	using ostream_impl = basic_ostream_impl<Stream, char>;
	template<class Stream>
	using buffered_istream_impl = basic_buffered_istream_impl<Stream, char>;
	template<class Stream>
	using buffered_ostream_impl = basic_buffered_ostream_impl<Stream, char>;

	using istream_reference = basic_istream_reference<char>;
	using ostream_reference = basic_ostream_reference<char>;
	using buffered_istream_reference = basic_buffered_istream_reference<char>;
	using buffered_ostream_reference = basic_buffered_ostream_reference<char>;
} // namespace cobra

#endif
