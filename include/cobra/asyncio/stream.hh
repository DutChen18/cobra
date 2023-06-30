#ifndef COBRA_ASYNCIO_STREAM_HH
#define COBRA_ASYNCIO_STREAM_HH

#include "cobra/asyncio/task.hh"

#include <string>
#include <optional>

namespace cobra {
	template<class CharT, class Traits>
	class basic_istream_tag;

	template<class CharT, class Traits>
	class basic_ostream_tag;

	template<class CharT, class Traits>
	class basic_buffered_istream_tag;

	template<class CharT, class Traits>
	class basic_buffered_ostream_tag;

	template<class Stream, class Tag>
	class basic_istream_tag_impl;

	template<class Stream, class Tag>
	class basic_ostream_tag_impl;

	template<class Stream, class Tag>
	class basic_buffered_istream_tag_impl;

	template<class Stream, class Tag>
	class basic_buffered_ostream_tag_impl;

	template<class CharT, class Traits = std::char_traits<CharT>>
	class basic_istream {
	public:
		using char_type = CharT;
		using traits_type = Traits;
		using tag_type = basic_istream_tag<CharT, Traits>;

		template<class Stream>
		constexpr static basic_istream_tag_impl<Stream, tag_type> tag;
	};

	template<class CharT, class Traits = std::char_traits<CharT>>
	class basic_ostream {
	public:
		using char_type = CharT;
		using traits_type = Traits;
		using tag_type = basic_ostream_tag<CharT, Traits>;

		template<class Stream>
		constexpr static basic_ostream_tag_impl<Stream, tag_type> tag;
	};

	template<class CharT, class Traits = std::char_traits<CharT>>
	class basic_buffered_istream : public basic_istream<CharT, Traits> {
	public:
		using tag_type = basic_buffered_istream_tag<CharT, Traits>;

		template<class Stream>
		constexpr static basic_buffered_istream_tag_impl<Stream, tag_type> tag;
	};

	template<class CharT, class Traits = std::char_traits<CharT>>
	class basic_buffered_ostream : public basic_ostream<CharT, Traits> {
	public:
		using tag_type = basic_buffered_ostream_tag<CharT, Traits>;

		template<class Stream>
		constexpr static basic_buffered_ostream_tag_impl<Stream, tag_type> tag;
	};

	template<class Stream, class Base = basic_istream<typename Stream::char_type, typename Stream::traits_type>>
	class istream_impl : public Base {
	public:
		using typename Base::char_type;

		[[nodiscard]] task<std::optional<char_type>> get() {
			Stream* self = static_cast<Stream*>(this);

			// TODO
		}
	};
	
	template<class Stream, class Base = basic_istream<typename Stream::char_type, typename Stream::traits_type>>
	class ostream_impl : public Base {
	public:
		using typename Base::char_type;

		[[nodiscard]] task<std::size_t> write_all(const char_type* data, std::size_t size) {
			Stream* self = static_cast<Stream*>(this);

			// TODO
		}
	};

	template<class Stream, class Base = basic_buffered_istream<typename Stream::char_type, typename Stream::traits_type>>
	class buffered_istream_impl : public istream_impl<Stream, Base> {};

	template<class Stream, class Base = basic_buffered_ostream<typename Stream::char_type, typename Stream::traits_type>>
	class buffered_ostream_impl : public ostream_impl<Stream, Base> {};

	template<class CharT, class Traits>
	class basic_istream_tag {
	public:
		using stream_type = basic_istream<CharT, Traits>;
		using typename stream_type::char_type;

		virtual ~basic_istream_tag() {}

		[[nodiscard]] virtual task<std::size_t> read(stream_type* stream, char_type* data, std::size_t size) const = 0;
		[[nodiscard]] virtual task<std::optional<char_type>> get(stream_type* stream) const = 0;
	};

	template<class CharT, class Traits>
	class basic_ostream_tag {
	public:
		using stream_type = basic_ostream<CharT, Traits>;
		using typename stream_type::char_type;

		virtual ~basic_ostream_tag() {}

		[[nodiscard]] virtual task<std::size_t> write(stream_type* stream, const char_type* data, std::size_t size) const = 0;
		[[nodiscard]] virtual task<void> flush(stream_type* stream) const = 0;
		[[nodiscard]] virtual task<std::size_t> write_all(stream_type* stream, const char_type* data, std::size_t size) const = 0;
	};

	template<class CharT, class Traits>
	class basic_buffered_istream_tag : public basic_istream_tag<CharT, Traits> {
	public:
		using stream_type = basic_istream<CharT, Traits>;
		using typename stream_type::char_type;

		[[nodiscard]] virtual task<std::optional<char_type>> peek(stream_type* stream) const = 0;
	};

	template<class CharT, class Traits>
	class basic_buffered_ostream_tag : public basic_ostream_tag<CharT, Traits> {
	public:
		using stream_type = basic_istream<CharT, Traits>;
		using typename stream_type::char_type;
	};

	template<class Stream, class Tag>
	class istream_tag_impl : public Tag {
	public:
		using typename Tag::stream_type;
		using typename Tag::char_type;

		[[nodiscard]] task<std::size_t> read(stream_type* stream, char_type* data, std::size_t size) const override {
			return static_cast<Stream*>(stream)->read(data, size);
		}

		[[nodiscard]] task<std::optional<char_type>> get(stream_type* stream) const override {
			return static_cast<Stream*>(stream)->get();
		}
	};

	template<class Stream, class Tag>
	class ostream_tag_impl : public Tag {
	public:
		using typename Tag::stream_type;
		using typename Tag::char_type;

		[[nodiscard]] task<std::size_t> write(stream_type* stream, const char_type* data, std::size_t size) const override {
			return static_cast<Stream*>(stream)->write(data, size);
		}

		[[nodiscard]] task<void> flush(stream_type* stream) const override {
			return static_cast<Stream*>(stream)->flush();
		}

		[[nodiscard]] task<std::size_t> write_all(stream_type* stream, const char_type* data, std::size_t size) const override {
			return static_cast<Stream*>(stream)->write_all(data, size);
		}
	};

	template<class Stream, class Tag>
	class buffered_istream_tag_impl : public istream_tag_impl<Stream, Tag> {
	public:
		using typename Tag::stream_type;
		using typename Tag::char_type;

		[[nodiscard]] task<std::optional<char_type>> peek(stream_type* stream) const override {
			return static_cast<Stream*>(stream)->get();
		}
	};

	template<class Stream, class Tag>
	class buffered_ostream_tag_impl : public ostream_tag_impl<Stream, Tag> {
	public:
		using typename Tag::stream_type;
		using typename Tag::char_type;
	};

	template<class Base>
	class stream_reference {
		Base* _ptr;
		const typename Base::tag_type* _tag;

	public:
		using typename Base::char_type;
		using typename Base::traits_type;

		template<class Stream>
		stream_reference(Stream& stream) {
			_ptr = &stream;
			_tag = &Base::template tag<Stream>;
		}

		template<class Stream>
		stream_reference(const stream_reference<Stream>& other) {
			_ptr = other._ptr;
			_tag = other._tag;
		}

		[[nodiscard]] task<std::size_t> read(char_type* data, std::size_t size) const
			requires std::is_base_of_v<basic_istream<char_type, traits_type>, Base>
		{
			return _tag->read(_ptr, data, size);
		}

		[[nodiscard]] task<std::optional<char_type>> get() const
			requires std::is_base_of_v<basic_istream<char_type, traits_type>, Base>
		{
			return _tag->get(_ptr);
		}

		[[nodiscard]] task<std::optional<char_type>> peek() const
			requires std::is_base_of_v<basic_buffered_istream<char_type, traits_type>, Base>
		{
			return _tag->peek(_ptr);
		}

		[[nodiscard]] task<std::size_t> write(const char_type* data, std::size_t size) const
			requires std::is_base_of_v<basic_ostream<char_type, traits_type>, Base>
		{
			return _tag->write(_ptr, data, size);
		}

		[[nodiscard]] task<void> flush() const
			requires std::is_base_of_v<basic_ostream<char_type, traits_type>, Base>
		{
			return _tag->flush(_ptr);
		}

		[[nodiscard]] task<std::size_t> write_all(const char_type* data, std::size_t size) const
			requires std::is_base_of_v<basic_ostream<char_type, traits_type>, Base>
		{
			return _tag->write_all(_ptr, data, size);
		}
	};

	template<class CharT, class Traits = std::char_traits<CharT>>
	using basic_istream_reference = stream_reference<basic_istream<CharT, Traits>>;
	template<class CharT, class Traits = std::char_traits<CharT>>
	using basic_ostream_reference = stream_reference<basic_ostream<CharT, Traits>>;
	template<class CharT, class Traits = std::char_traits<CharT>>
	using basic_buffered_istream_reference = stream_reference<basic_buffered_istream<CharT, Traits>>;
	template<class CharT, class Traits = std::char_traits<CharT>>
	using basic_buffered_ostream_reference = stream_reference<basic_buffered_ostream<CharT, Traits>>;

	using istream = basic_istream<char>;
	using ostream = basic_ostream<char>;
	using buffered_istream = basic_buffered_istream<char>;
	using buffered_ostream = basic_buffered_ostream<char>;

	using istream_reference = basic_istream_reference<char>;
	using ostream_reference = basic_ostream_reference<char>;
	using buffered_istream_reference = basic_buffered_istream_reference<char>;
	using buffered_ostream_reference = basic_buffered_ostream_reference<char>;
}

#endif
