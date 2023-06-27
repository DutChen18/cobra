#ifndef COBRA_ASYNCIO_STREAM_HH
#define COBRA_ASYNCIO_STREAM_HH

#include "cobra/asyncio/task.hh"

#include <optional>
#include <string>

namespace cobra {
	template <class CharT, class Traits = std::char_traits<CharT>>
	class basic_stream {
	public:
		using char_type = CharT;
		using traits_type = Traits;
		using int_type = typename Traits::int_type;
		using pos_type = typename Traits::pos_type;
		using off_type = typename Traits::off_type;

		virtual ~basic_stream() {}
	};

	template <class CharT, class Traits = std::char_traits<CharT>>
	class basic_istream : public basic_stream<CharT, Traits> {
	public:
		using typename basic_stream<CharT, Traits>::char_type;
		using typename basic_stream<CharT, Traits>::traits_type;
		using typename basic_stream<CharT, Traits>::int_type;
		using typename basic_stream<CharT, Traits>::pos_type;
		using typename basic_stream<CharT, Traits>::off_type;

		virtual task<std::size_t> read(char_type* data, std::size_t size) = 0;

		virtual task<std::optional<char_type>> get() {
			char_type result;

			if (co_await read(&result, 1) == 1) {
				co_return result;
			} else {
				co_return std::nullopt;
			}
		}
	};

	template <class CharT, class Traits = std::char_traits<CharT>>
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

	template<class CharT, class Traits = std::char_traits<CharT>>
	class basic_buffered_istream : public basic_istream<CharT, Traits> {
	public:
		using typename basic_istream<CharT, Traits>::char_type;
		using typename basic_istream<CharT, Traits>::traits_type;
		using typename basic_istream<CharT, Traits>::int_type;
		using typename basic_istream<CharT, Traits>::pos_type;
		using typename basic_istream<CharT, Traits>::off_type;

		virtual task<std::optional<char_type>> peek() = 0;
	};

	template<class BufferedStream>
	class basic_buffered_istream_reference : public basic_buffered_istream<typename BufferedStream::char_type, typename BufferedStream::traits_type> {
		BufferedStream& _stream;

	public:
		basic_buffered_istream_reference(basic_buffered_istream_reference& other) : _stream(other._stream) {}
		basic_buffered_istream_reference(BufferedStream& stream) : _stream(stream) {}

		using char_type = typename BufferedStream::char_type;
		using traits_type = typename BufferedStream::traits_type;
		using int_type = typename BufferedStream::int_type;
		using pos_type = typename BufferedStream::pos_type;
		using off_type = typename BufferedStream::off_type;

		task<std::size_t> read(char_type* data, std::size_t size) override {
			return _stream.read(data, size);
		}

		virtual task<std::optional<char_type>> peek() override {
			return _stream.peek();
		}
	};

	template<class BufferedStream>
	basic_buffered_istream_reference<BufferedStream> wrap_stream(BufferedStream& stream) {
		return basic_buffered_istream_reference<BufferedStream>(stream);
	}

	template<class CharT, class Traits = std::char_traits<CharT>>
	class basic_buffered_ostream : public basic_ostream<CharT, Traits> {
	public:
		using typename basic_ostream<CharT, Traits>::char_type;
		using typename basic_ostream<CharT, Traits>::traits_type;
		using typename basic_ostream<CharT, Traits>::int_type;
		using typename basic_ostream<CharT, Traits>::pos_type;
		using typename basic_ostream<CharT, Traits>::off_type;
	};

	template <class Stream>
	class basic_istream_buffer : public basic_buffered_istream<typename Stream::char_type, typename Stream::traits_type> {
		Stream _stream;

	public:
		using char_type = typename Stream::char_type;
		using traits_type = typename Stream::traits_type;
		using int_type = typename Stream::int_type;
		using pos_type = typename Stream::pos_type;
		using off_type = typename Stream::off_type;

		task<std::size_t> read(char_type* data, std::size_t size) override {
			return _stream.read(data, size);
		}

		task<std::optional<char_type>> peek() {
			co_return std::nullopt; // TODO
		}
	};

	template <class Stream>
	class basic_ostream_buffer : public basic_buffered_ostream<typename Stream::char_type, typename Stream::traits_type> {
		Stream _stream;

	public:
		using char_type = typename Stream::char_type;
		using traits_type = typename Stream::traits_type;
		using int_type = typename Stream::int_type;
		using pos_type = typename Stream::pos_type;
		using off_type = typename Stream::off_type;

		task<std::size_t> write(const char_type* data, std::size_t size) override {
			return _stream.write(data, size);
		}

		task<void> flush() override {
			return _stream.flush();
		}
	};

	using stream = basic_stream<char>;
	using istream = basic_istream<char>;
	using ostream = basic_ostream<char>;
	using buffered_istream = basic_buffered_istream<char>;
	using buffered_ostream = basic_buffered_ostream<char>;
} // namespace cobra

#endif
