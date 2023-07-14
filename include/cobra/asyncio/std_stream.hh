#ifndef COBRA_STD_STREAM_HH
#define COBRA_STD_STREAM_HH

#include "cobra/asyncio/stream.hh"

#include <iostream>

namespace cobra {
	template <class Stream>
	class std_istream : public basic_istream_impl<std_istream<Stream>, typename Stream::char_type, typename Stream::traits_type> {
		using base = basic_istream_impl<std_istream<Stream>, typename Stream::char_type, typename Stream::traits_type>;

		Stream _stream;

	public:
		using typename base::char_type;

		std_istream(Stream&& stream) : _stream(std::move(stream)) {
			_stream.exceptions(Stream::badbit);
		}

		task<std::size_t> read(char_type* data, std::size_t size) {
			_stream.read(data, size);
			co_return _stream.gcount();
		}

		Stream& inner() {
			return _stream;
		}
	};

	template <class Stream>
	class std_ostream : public basic_ostream_impl<std_ostream<Stream>, typename Stream::char_type, typename Stream::traits_type> {
		using base = basic_ostream_impl<std_ostream<Stream>, typename Stream::char_type, typename Stream::traits_type>;

		Stream _stream;

	public:
		using typename base::char_type;

		std_ostream(Stream&& stream) : _stream(std::move(stream)) {
			_stream.exceptions(Stream::badbit);
		}

		task<std::size_t> write(const char_type* data, std::size_t size) {
			_stream.write(data, size);
			co_return size;
		}

		task<void> flush() {
			_stream.flush();
			co_return;
		}

		Stream& inner() {
			return _stream;
		}
	};

	template <class CharT, class Traits = std::char_traits<CharT>>
	class basic_std_istream_reference : public basic_istream_impl<basic_std_istream_reference<CharT, Traits>, CharT, Traits> {
		using base = basic_istream_impl<basic_std_istream_reference<CharT, Traits>, CharT, Traits>;

		std::reference_wrapper<std::basic_istream<CharT, Traits>> _stream;

	public:
		using typename base::char_type;

		basic_std_istream_reference(std::basic_istream<CharT, Traits>& stream) : _stream(stream) {
			_stream.get().exceptions(std::basic_istream<CharT, Traits>::badbit);
		}

		task<std::size_t> read(char_type* data, std::size_t size) {
			_stream.get().read(data, size);
			co_return _stream.get().gcount();
		}

		std::basic_istream<CharT, Traits>& inner() {
			return _stream.get();
		}
	};

	template <class CharT, class Traits = std::char_traits<CharT>>
	class basic_std_ostream_reference : public basic_ostream_impl<basic_std_ostream_reference<CharT, Traits>, CharT, Traits> {
		using base = basic_ostream_impl<basic_std_ostream_reference<CharT, Traits>, CharT, Traits>;

		std::reference_wrapper<std::basic_ostream<CharT, Traits>> _stream;

	public:
		using typename base::char_type;

		basic_std_ostream_reference(std::basic_ostream<CharT, Traits>& stream) : _stream(stream) {
			_stream.get().exceptions(std::basic_ostream<CharT, Traits>::badbit);
		}

		task<std::size_t> write(const char_type* data, std::size_t size) {
			_stream.get().write(data, size);
			co_return size;
		}

		task<void> flush() {
			_stream.get().flush();
			co_return;
		}

		std::basic_ostream<CharT, Traits>& inner() {
			return _stream.get();
		}
	};

	using std_istream_reference = basic_std_istream_reference<char>;
	using std_ostream_reference = basic_std_ostream_reference<char>;
}

#endif
