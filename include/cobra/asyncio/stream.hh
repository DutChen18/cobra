#ifndef COBRA_ASYNCIO_STREAM_HH
#define COBRA_ASYNCIO_STREAM_HH

#include "cobra/asyncio/task.hh"

#include <optional>
#include <string>
#include <type_traits>
#include <variant>
#include <concepts>

namespace cobra {
	template <class T>
	concept Stream = requires(T t) {
		typename T::char_type;
		typename T::traits_type;
		typename T::int_type;
		typename T::pos_type;
		typename T::off_type;
	};

	template <class T, class CharT, class SizeT>
	concept AsyncInput = requires(T& t, CharT* data, SizeT size) {
		{ t.read(data, size) } -> std::convertible_to<task<SizeT>>;
		{ t.get() } -> std::convertible_to<task<std::optional<CharT>>>;
		{ t.read_all(data, size) } -> std::convertible_to<task<SizeT>>;
	};

	template <class T, class CharT, class SizeT>
	concept AsyncOutput = requires(T& t, const CharT* data, SizeT size) {
		{ t.write(data, size) } -> std::convertible_to<task<SizeT>>;
		{ t.flush() } -> std::convertible_to<task<void>>;
		{ t.write_all(data, size) } -> std::convertible_to<task<SizeT>>;
	};

	template <class T, class CharT, class SizeT>
	concept AsyncBufferedInput = requires(T& t, SizeT size) {
		{ t.fill_buf() } -> std::convertible_to<task<std::pair<const CharT*, SizeT>>>;
		{ t.consume(size) } -> std::convertible_to<void>;
		{ t.peek() } -> std::convertible_to<task<std::optional<CharT>>>;
	};

	template <class T, class CharT, class SizeT>
	concept AsyncBufferedOutput = true;

	template <class T>
	concept AsyncInputStream = Stream<T> && AsyncInput<T, typename T::char_type, std::size_t>;

	template <class T>
	concept AsyncOutputStream = Stream<T> && AsyncOutput<T, typename T::char_type, std::size_t>;

	template <class T>
	concept AsyncBufferedInputStream = AsyncInputStream<T> && AsyncBufferedInput<T, typename T::char_type, std::size_t>;

	template <class T>
	concept AsyncBufferedOutputStream = AsyncOutputStream<T> && AsyncBufferedOutput<T, typename T::char_type, std::size_t>;

	template <class CharT, class Traits>
	class basic_istream_tag;
	template <class CharT, class Traits>
	class basic_ostream_tag;
	template <class CharT, class Traits>
	class basic_buffered_istream_tag;
	template <class CharT, class Traits>
	class basic_buffered_ostream_tag;
	
	template <class Stream, class Tag>
	class istream_tag_impl;
	template <class Stream, class Tag>
	class ostream_tag_impl;
	template <class Stream, class Tag>
	class buffered_istream_tag_impl;
	template <class Stream, class Tag>
	class buffered_ostream_tag_impl;

	enum class stream_error {
		incomplete_read,
		incomplete_write,
	};

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

		template <class Stream>
		using tag_impl_type = istream_tag_impl<Stream, basic_istream_tag<CharT, Traits>>;
	};

	template <class CharT, class Traits = std::char_traits<CharT>>
	class basic_ostream : public basic_stream<CharT, Traits> {
	public:
		using tag_type = basic_ostream_tag<CharT, Traits>;
		using base_type = basic_ostream;

		template <class Stream>
		using tag_impl_type = ostream_tag_impl<Stream, basic_ostream_tag<CharT, Traits>>;
	};

	template <class CharT, class Traits = std::char_traits<CharT>>
	class basic_buffered_istream : public basic_istream<CharT, Traits> {
	public:
		using tag_type = basic_buffered_istream_tag<CharT, Traits>;
		using base_type = basic_buffered_istream;

		template <class Stream>
		using tag_impl_type = buffered_istream_tag_impl<Stream, basic_buffered_istream_tag<CharT, Traits>>;
	};

	template <class CharT, class Traits = std::char_traits<CharT>>
	class basic_buffered_ostream : public basic_ostream<CharT, Traits> {
	public:
		using tag_type = basic_buffered_ostream_tag<CharT, Traits>;
		using base_type = basic_buffered_ostream;

		template <class Stream>
		using tag_impl_type = buffered_ostream_tag_impl<Stream, basic_buffered_ostream_tag<CharT, Traits>>;
	};

	template <class CharT, class Traits>
	class basic_istream_tag {
	public:
		using stream_type = basic_istream<CharT, Traits>;
		using char_type = typename stream_type::char_type;

		virtual ~basic_istream_tag() {}

		virtual task<std::size_t> read(stream_type* stream, char_type* data, std::size_t size) const = 0;
		virtual task<std::optional<char_type>> get(stream_type* stream) const = 0;
		virtual task<std::size_t> read_all(stream_type* stream, char_type* data, std::size_t size) const = 0;
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

		virtual task<std::pair<const char_type*, std::size_t>> fill_buf(stream_type* stream) const = 0;
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
		using tag_type = basic_istream_tag<typename Stream::char_type, typename Stream::traits_type>;
		using typename tag_type::char_type;
		using typename tag_type::stream_type;

		task<std::size_t> read(stream_type* stream, char_type* data, std::size_t size) const override {
			return static_cast<Stream*>(stream)->read(data, size);
		}

		task<std::optional<char_type>> get(stream_type* stream) const override {
			return static_cast<Stream*>(stream)->get();
		}

		task<std::size_t> read_all(stream_type* stream, char_type* data, std::size_t size) const override {
			return static_cast<Stream*>(stream)->read_all(data, size);
		}
	};

	template <class Stream, class Tag>
	class ostream_tag_impl : public Tag {
	public:
		using tag_type = basic_ostream_tag<typename Stream::char_type, typename Stream::traits_type>;
		using typename tag_type::char_type;
		using typename tag_type::stream_type;

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
		using tag_type = basic_buffered_istream_tag<typename Stream::char_type, typename Stream::traits_type>;
		using typename tag_type::char_type;
		using typename tag_type::stream_type;

		task<std::pair<const char_type*, std::size_t>> fill_buf(stream_type* stream) const override {
			return static_cast<Stream*>(stream)->fill_buf();
		}

		void consume(stream_type* stream, std::size_t size) const override {
			return static_cast<Stream*>(stream)->consume(size);
		}

		task<std::optional<char_type>> peek(stream_type* stream) const override {
			return static_cast<Stream*>(stream)->peek();
		}
	};

	template <class Stream, class Tag>
	class buffered_ostream_tag_impl : public ostream_tag_impl<Stream, Tag> {
	public:
		using tag_type = basic_buffered_ostream_tag<typename Stream::char_type, typename Stream::traits_type>;
		using typename tag_type::char_type;
		using typename tag_type::stream_type;
	};

	template <class Stream, class CharT, class Traits = std::char_traits<CharT>, class Base = basic_istream<CharT, Traits>>
	class basic_istream_impl : public Base {
	public:
		using typename Base::char_type;

		task<std::optional<char_type>> get() {
			Stream* self = static_cast<Stream*>(this);
			char_type result;

			if (co_await self->read(&result, 1) == 1) {
				co_return result;
			} else {
				co_return std::nullopt;
			}
		}

		task<std::size_t> read_all(char_type* data, std::size_t size) {
			Stream* self = static_cast<Stream*>(this);
			std::size_t index = 0;
			
			while (index < size) {
				std::size_t ret = co_await self->read(data + index, size - index);
				index += ret;

				if (ret == 0) {
					throw stream_error::incomplete_read;
				}
			}

			co_return index;
		}
	};

	template <class Stream, class CharT, class Traits = std::char_traits<CharT>, class Base = basic_ostream<CharT, Traits>>
	class basic_ostream_impl : public Base {
	public:
		using typename Base::char_type;

		task<std::size_t> write_all(const char_type* data, std::size_t size) {
			Stream* self = static_cast<Stream*>(this);
			std::size_t index = 0;

			while (index < size) {
				std::size_t ret = co_await self->write(data + index, size - index);
				index += ret;

				if (ret == 0) {
					throw stream_error::incomplete_write;
				}
			}

			co_return index;
		}
	};

	template <class Stream, class CharT, class Traits = std::char_traits<CharT>,
			  class Base = basic_buffered_istream<CharT, Traits>>
	class basic_buffered_istream_impl : public basic_istream_impl<Stream, CharT, Traits, Base> {
	public:
		using typename Base::char_type;

		task<std::size_t> read(char_type* data, std::size_t size) {
			Stream* self = static_cast<Stream*>(this);
			auto [buffer, buffer_size] = co_await self->fill_buf();
			auto count = std::min(size, buffer_size);
			std::copy(buffer, buffer + count, data);
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
	};

	template <class Wrapper, class Base>
	class stream_wrapper : public Base::base_type {
	public:
		using typename Base::base_type::char_type;
		using typename Base::base_type::traits_type;

		task<std::size_t> read(char_type* data, std::size_t size) const
			requires std::is_base_of_v<basic_istream<char_type, traits_type>, Base>
		{
			const Wrapper* wrapper = static_cast<const Wrapper*>(this);
			return wrapper->tag()->read(wrapper->ptr(), data, size);
		}

		task<std::optional<char_type>> get() const
			requires std::is_base_of_v<basic_istream<char_type, traits_type>, Base>
		{
			const Wrapper* wrapper = static_cast<const Wrapper*>(this);
			return wrapper->tag()->get(wrapper->ptr());
		}

		task<std::size_t> read_all(char_type* data, std::size_t size) const
			requires std::is_base_of_v<basic_istream<char_type, traits_type>, Base>
		{
			const Wrapper* wrapper = static_cast<const Wrapper*>(this);
			return wrapper->tag()->read_all(wrapper->ptr(), data, size);
		}

		task<std::pair<const char_type*, std::size_t>> fill_buf() const
			requires std::is_base_of_v<basic_buffered_istream<char_type, traits_type>, Base>
		{
			const Wrapper* wrapper = static_cast<const Wrapper*>(this);
			return wrapper->tag()->fill_buf(wrapper->ptr());
		}

		void consume(std::size_t size) const
			requires std::is_base_of_v<basic_buffered_istream<char_type, traits_type>, Base>
		{
			const Wrapper* wrapper = static_cast<const Wrapper*>(this);
			return wrapper->tag()->consume(wrapper->ptr(), size);
		}

		task<std::optional<char_type>> peek() const
			requires std::is_base_of_v<basic_buffered_istream<char_type, traits_type>, Base>
		{
			const Wrapper* wrapper = static_cast<const Wrapper*>(this);
			return wrapper->tag()->peek(wrapper->ptr());
		}

		task<std::size_t> write(const char_type* data, std::size_t size) const
			requires std::is_base_of_v<basic_ostream<char_type, traits_type>, Base>
		{
			const Wrapper* wrapper = static_cast<const Wrapper*>(this);
			return wrapper->tag()->write(wrapper->ptr(), data, size);
		}

		task<void> flush() const
			requires std::is_base_of_v<basic_ostream<char_type, traits_type>, Base>
		{
			const Wrapper* wrapper = static_cast<const Wrapper*>(this);
			return wrapper->tag()->flush(wrapper->ptr());
		}

		task<std::size_t> write_all(const char_type* data, std::size_t size) const
			requires std::is_base_of_v<basic_ostream<char_type, traits_type>, Base>
		{
			const Wrapper* wrapper = static_cast<const Wrapper*>(this);
			return wrapper->tag()->write_all(wrapper->ptr(), data, size);
		}
	};

	template <class Base>
	class stream_reference;

	namespace detail {
		template <class Base, class Stream>
		const typename Base::tag_type* tag() {
			static typename Base::template tag_impl_type<Stream> tag;
			return &tag;
		}

		template <class T>
		struct is_stream_reference : std::false_type {};

		template <class T>
		struct is_stream_reference<stream_reference<T>> : std::true_type {};

		template <class T>
		constexpr bool is_stream_reference_v = is_stream_reference<T>::value;
	}

	template <class Base, std::derived_from<Base> Stream>
	class stream_ref : public stream_wrapper<stream_ref<Base, Stream>, Base> {
		Stream* _stream;

	public:
		stream_ref(Stream& stream) {
			_stream = &stream;
		}

		template <std::derived_from<Base> Base2>
		stream_ref(const stream_ref<Base2, Stream>& other) {
			_stream = other.ptr();
		}


		stream_ref copy() const {
			return *this;
		}

		Stream* ptr() const {
			return _stream;
		}

		const typename Base::tag_type* tag() const {
			return detail::tag<Base, Stream>();
		}
	};

	template <class Base>
	class stream_reference : public stream_wrapper<stream_reference<Base>, Base> {
		Base* _ptr;
		const typename Base::tag_type* _tag;

	public:
		template <std::derived_from<Base> Stream>
		stream_reference(Stream& stream) requires (!detail::is_stream_reference_v<Stream>) {
			_ptr = &stream;
			_tag = detail::tag<Base, Stream>();
		}

		template <std::derived_from<Base> Base2, std::derived_from<Base2> Stream>
		stream_reference(const stream_ref<Base2, Stream>& other) {
			_ptr = other.ptr();
			_tag = other.tag();
		}

		template <std::derived_from<Base> Base2>
		stream_reference(const stream_reference<Base2>& other) {
			_ptr = other.ptr();
			_tag = other.tag();
		}

		stream_reference copy() const {
			return *this;
		}

		Base* ptr() const {
			return _ptr;
		}

		const typename Base::tag_type* tag() const {
			return _tag;
		}
	};

	template <class Base, std::derived_from<Base>... Streams>
	class stream_variant : public stream_wrapper<stream_variant<Base, Streams...>, Base> {
		mutable std::variant<Streams...> _stream;

	public:
		template <std::derived_from<Base> Stream>
		stream_variant(Stream&& stream) : _stream(std::move(stream)) {
		}

		stream_variant(stream_variant&& other) : _stream(std::move(other._stream)) {
		}

		stream_variant& operator=(stream_variant other) {
			std::swap(_stream, other._stream);
			return *this;
		}

		std::variant<Streams...>& variant() {
			return _stream;
		}

		const std::variant<Streams...>& variant() const {
			return _stream;
		}

		Base* ptr() const {
			return std::visit([](auto& stream) { return static_cast<Base*>(&stream); }, _stream);
		}

		const typename Base::tag_type* tag() const {
			return std::visit([](auto& stream) { return detail::tag<Base, std::remove_cvref_t<decltype(stream)>>(); }, _stream);
		}
	};

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

	template <class Stream, class CharT, class Traits = std::char_traits<CharT>>
	using basic_istream_ref = stream_ref<basic_istream<CharT, Traits>, Stream>;
	template <class Stream, class CharT, class Traits = std::char_traits<CharT>>
	using basic_ostream_ref = stream_ref<basic_ostream<CharT, Traits>, Stream>;
	template <class Stream, class CharT, class Traits = std::char_traits<CharT>>
	using basic_buffered_istream_ref = stream_ref<basic_buffered_istream<CharT, Traits>, Stream>;
	template <class Stream, class CharT, class Traits = std::char_traits<CharT>>
	using basic_buffered_ostream_ref = stream_ref<basic_buffered_ostream<CharT, Traits>, Stream>;

	template <class CharT, class Traits = std::char_traits<CharT>>
	using basic_istream_reference = stream_reference<basic_istream<CharT, Traits>>;
	template <class CharT, class Traits = std::char_traits<CharT>>
	using basic_ostream_reference = stream_reference<basic_ostream<CharT, Traits>>;
	template <class CharT, class Traits = std::char_traits<CharT>>
	using basic_buffered_istream_reference = stream_reference<basic_buffered_istream<CharT, Traits>>;
	template <class CharT, class Traits = std::char_traits<CharT>>
	using basic_buffered_ostream_reference = stream_reference<basic_buffered_ostream<CharT, Traits>>;

	template <class CharT, class Traits = std::char_traits<CharT>, class... Streams>
	using basic_istream_variant = stream_variant<basic_istream<CharT, Traits>, Streams...>;
	template <class CharT, class Traits = std::char_traits<CharT>, class... Streams>
	using basic_ostream_variant = stream_variant<basic_ostream<CharT, Traits>, Streams...>;
	template <class CharT, class Traits = std::char_traits<CharT>, class... Streams>
	using basic_buffered_istream_variant = stream_variant<basic_buffered_istream<CharT, Traits>, Streams...>;
	template <class CharT, class Traits = std::char_traits<CharT>, class... Streams>
	using basic_buffered_ostream_variant = stream_variant<basic_buffered_ostream<CharT, Traits>, Streams...>;

	template <class... Streams>
	using istream_variant = basic_istream_variant<char, std::char_traits<char>, Streams...>;
	template <class... Streams>
	using ostream_variant = basic_ostream_variant<char, std::char_traits<char>, Streams...>;
	template <class... Streams>
	using buffered_istream_variant = basic_buffered_istream_variant<char, std::char_traits<char>, Streams...>;
	template <class... Streams>
	using buffered_ostream_variant = basic_buffered_ostream_variant<char, std::char_traits<char>, Streams...>;

	template <class Stream>
	using istream_ref = basic_istream_ref<Stream, char>;
	template <class Stream>
	using ostream_ref = basic_ostream_ref<Stream, char>;
	template <class Stream>
	using buffered_istream_ref = basic_buffered_istream_ref<Stream, char>;
	template <class Stream>
	using buffered_ostream_ref = basic_buffered_ostream_ref<Stream, char>;

	using istream_reference = basic_istream_reference<char>;
	using ostream_reference = basic_ostream_reference<char>;
	using buffered_istream_reference = basic_buffered_istream_reference<char>;
	using buffered_ostream_reference = basic_buffered_ostream_reference<char>;

	template <class Stream>
	istream_ref<Stream> make_istream_ref(Stream& stream) {
		return stream;
	}

	template <class Stream>
	ostream_ref<Stream> make_ostream_ref(Stream& stream) {
		return stream;
	}

	template <class Stream>
	buffered_istream_ref<Stream> make_buffered_istream_ref(Stream& stream) {
		return stream;
	}

	template <class Stream>
	buffered_ostream_ref<Stream> make_buffered_ostream_ref(Stream& stream) {
		return stream;
	}

	template <class CharT, class Traits = std::char_traits<CharT>>
	task<void> pipe(basic_buffered_istream_reference<CharT, Traits> istream, basic_ostream_reference<CharT, Traits> ostream) {
		while (true) {
			auto [buffer, buffer_size] = co_await istream.fill_buf();

			if (buffer_size == 0) {
				break;
			}

			co_await ostream.write_all(buffer, buffer_size);
			co_await ostream.flush(); // ODOT: shouldn't flush every iteration
			istream.consume(buffer_size);
		}
	}
} // namespace cobra

#endif
