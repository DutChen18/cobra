#ifndef COBRA_STD_STREAM_HH
#define COBRA_STD_STREAM_HH

#include "cobra/asyncio/stream.hh"
#include "cobra/file.hh"

#include <iostream>

namespace cobra {
	template <class Stream>
	class std_istream : public basic_istream_impl<std_istream<Stream>, typename Stream::char_type, typename Stream::traits_type> {
		using base = basic_istream_impl<std_istream<Stream>, typename Stream::char_type, typename Stream::traits_type>;

		Stream _stream;

	public:
		using typename base::char_type;

		std_istream(Stream&& stream) : _stream(std::move(stream)) {
			_stream.exceptions(Stream::badbit | Stream::failbit);
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
			_stream.exceptions(Stream::badbit | Stream::failbit);
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
			_stream.get().exceptions(std::basic_istream<CharT, Traits>::badbit | std::basic_istream<CharT, Traits>::failbit);
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
			_stream.get().exceptions(std::basic_ostream<CharT, Traits>::badbit | std::basic_ostream<CharT, Traits>::failbit);
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

	class file_istream : public istream_impl<file_istream> {
		std::FILE* _file;

	public:
		using istream_impl<file_istream>::char_type;

		inline file_istream(const char* name) {
			_file = std::fopen(name, "rb");
			std::setbuf(_file, NULL);
		}

		inline file_istream(const file_istream& other) = delete;

		inline file_istream(file_istream&& other) {
			_file = std::exchange(other._file, nullptr);
		}

		inline ~file_istream() {
			if (_file) {
				std::fclose(_file);
			}
		}

		inline file_istream& operator=(file_istream other) {
			std::swap(_file, other._file);
			return *this;
		}

		inline operator bool() const {
			return _file;
		}

		inline task<std::size_t> read(char_type* data, std::size_t size) {
			co_return std::fread(data, 1, size, _file);
		}
	};

	class file_ostream : public ostream_impl<file_ostream> {
		std::FILE* _file;

	public:
		using ostream_impl<file_ostream>::char_type;

		inline file_ostream(const char* name) {
			_file = std::fopen(name, "wb");
			std::setbuf(_file, NULL);
		}

		inline file_ostream(const file_ostream& other) = delete;

		inline file_ostream(file_ostream&& other) {
			_file = std::exchange(other._file, nullptr);
		}

		inline ~file_ostream() {
			if (_file) {
				std::fclose(_file);
			}
		}

		inline file_ostream& operator=(file_ostream other) {
			std::swap(_file, other._file);
			return *this;
		}

		inline operator bool() const {
			return _file;
		}

		inline task<std::size_t> write(const char_type* data, std::size_t size) {
			co_return std::fwrite(data, 1, size, _file);
		}

		inline task<void> flush() {
			co_return;
		}
	};

	using std_istream_reference = basic_std_istream_reference<char>;
	using std_ostream_reference = basic_std_ostream_reference<char>;
}

#endif
