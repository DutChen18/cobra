#ifndef COBRA_ASIO_HH
#define COBRA_ASIO_HH

#include "cobra/future.hh"
#include "cobra/optional.hh"
#include <algorithm>
#include <exception>
#include <ios>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <cstring>

namespace cobra {

	template <class CharT, class Traits = std::char_traits<CharT>>
	class basic_ios {
	protected:
		basic_ios() = default;
		virtual ~basic_ios();

	public:
		using char_type = CharT;
		using traits_type = Traits;
		using int_type = typename Traits::int_type;
		using pos_type = typename Traits::pos_type;
		using off_type = typename Traits::off_type;
	};

	template <class CharT, class Traits = std::char_traits<CharT>>
	class basic_istream : public virtual basic_ios<CharT, Traits> {
		using _base = basic_ios<CharT, Traits>;

	public:
		using char_type = CharT;
		using traits_type = Traits;
		using int_type = typename Traits::int_type;
		using pos_type = typename Traits::pos_type;
		using off_type = typename Traits::off_type;

		virtual future<std::size_t> read(char_type* dst, std::size_t count) = 0;
	};

	template <class CharT, class Traits = std::char_traits<CharT>>
	class basic_ostream : public virtual basic_ios<CharT, Traits> {
		using _base = basic_ios<CharT, Traits>;

	public:
		using char_type = CharT;
		using traits_type = Traits;
		using int_type = typename Traits::int_type;
		using pos_type = typename Traits::pos_type;
		using off_type = typename Traits::off_type;
		
		virtual future<std::size_t> write(const char_type* data, std::size_t count) = 0;
		virtual future<> flush() = 0;

		virtual future<std::size_t> write_all(const char_type* data, std::size_t count) {
			std::size_t progress = 0;
			return async_while(capture([this, data, count](std::size_t& progress) {
				return write(data + progress, count - progress).template map<optional<std::size_t>>([&progress, count](std::size_t nwritten) {
					if (nwritten == 0) {
						return some<std::size_t>(progress);
					} else if (progress += nwritten == count) {
						return some<std::size_t>(count);
					}
					return none<std::size_t>();
				});
			}, std::move(progress)));
		}

	};

	template <class CharT, class Traits = std::char_traits<CharT>>
	class basic_iostream : public basic_istream<CharT, Traits>, public basic_ostream<CharT, Traits> {
	public:
		using char_type = CharT;
		using traits_type = Traits;
		using int_type = typename Traits::int_type;
		using pos_type = typename Traits::pos_type;
		using off_type = typename Traits::off_type;
	};

	template <class CharT, class Traits = std::char_traits<CharT>, class Allocator = std::allocator<CharT>>
	class buffered_istream : public virtual basic_istream<CharT, Traits> {
		using stream_type = basic_istream<CharT, Traits>;
		using buffer_type = std::vector<CharT, Allocator>;
		using _base = basic_istream<CharT, Traits>;

	public:
		using char_type = CharT;
		using traits_type = Traits;
		using int_type = typename Traits::int_type;
		using pos_type = typename Traits::pos_type;
		using off_type = typename Traits::off_type;
		using allocator_type = Allocator;
		using size_type = typename buffer_type::size_type;

	protected:
		stream_type _stream;
		buffer_type _buffer;
		size_type _size;
		size_type _read_offset;

		size_type _capacity;
	public:
		buffered_istream() = delete;

		buffered_istream(stream_type&& stream, size_type capacity = 1024) noexcept : _stream(std::move(stream)), _capacity(capacity) {}

		buffered_istream(buffered_istream&& other) noexcept : buffered_istream(std::move(other._stream)) {
			other._capacity = 0;
			std::swap(_buffer, other._buffer);
			std::swap(_size, other._size);
			std::swap(_read_offset, other._read_offset);
		}

		size_type in_avail() const {
			return egptr() - gptr();
		}

		virtual future<size_type> read(char_type* dst, size_type count) {
			fill_buf().then([this, dst, count]() {
				return read_now(dst, count);
			});
		}

		virtual future<int_type> get() {
			return fill_buf().then([this]() {
				return get_now();
			});
		}

		virtual future<int_type> peek() {
			return fill_buf().then([this]() {
				return peek_now();
			});
		}

	protected:
		char_type *gptr() const {
			return _buffer.data() + read_offset();
		}

		char_type *egptr() const {
			return _buffer.data() + size();
		}

		bool empty() const {
			return in_avail() == 0;
		}

		size_type size() const {
			return _size;
		}

		size_type read_offset() const {
			return _read_offset;
		}

		size_type capacity() const {
			return _capacity;
		}

		void consume(size_type count) {
			_read_offset += count;
		}

		future<size_type> fill_buf() {
			if (empty()) {
				_buffer.clear();
				return _stream.read(_buffer.data(), capacity()).then([this](size_type nread) {
					_size = nread;
					return nread;
				});
			}
			return in_avail();
		}

		size_type read_now(char_type* dst, size_type count) {
			size_type capacity = count < in_avail() ? count : in_avail();
			std::memcpy(dst, gptr(), capacity);
			consume(capacity);
		}

		int_type peek_now() {
			if (in_avail() == 0) //TODO debug code, remove
				throw std::runtime_error("would block");
			const int_type result = *gptr();
			return result;
		}

		int_type get_now() {
			const int_type result = peek_now();
			consume(1);
			return result;
		}
	};

	template <class CharT, class Traits = std::char_traits<CharT>, class Allocator = std::allocator<CharT>>
	class buffered_ostream : public virtual basic_ostream<CharT, Traits> {
	protected:
		using stream_type = basic_ostream<CharT, Traits>;
		using buffer_type = std::vector<CharT, Allocator>;
		using _base = basic_ostream<CharT, Traits>;

	public:
		using char_type = CharT;
		using traits_type = Traits;
		using int_type = typename Traits::int_type;
		using pos_type = typename Traits::pos_type;
		using off_type = typename Traits::off_type;
		using allocator_type = Allocator;
		using size_type = typename buffer_type::size_type;

	protected:
		stream_type _stream;
		buffer_type _buffer;
		size_type _size;
		size_type _write_offset;

		size_type _capacity;

	public:
		buffered_ostream() = delete;

		buffered_ostream(stream_type&& stream, size_type capacity = 1024) noexcept : _stream(std::move(stream)), _capacity(capacity) {}

		buffered_ostream(buffered_ostream&& other) noexcept : buffered_ostream(std::move(other._stream)) {
			other._capacity = 0;
			std::swap(_buffer, other._buffer);
			std::swap(_size, other._size);
			std::swap(_write_offset, other._write_offset);
		}

		virtual future<size_type> write(const char_type* data, size_type count) {
			if (count > capacity()) {
				return sync().template then<size_type>([this, data, count]() {
					return _stream.write_all(data, count);
				});
			}
			const size_type nwritten = write_now(data, count);
			
			if (nwritten < count) {
				return sync().template map<size_type>([this, data, count, nwritten]() {
					write_now(data + nwritten, count - nwritten);
					return count;
				});
			}
			return count;
		}

		virtual future<> flush() {
			return sync().then([this]() {
				return _stream.flush();
			});
		}

		virtual future<> put(char_type ch) {
			return write(&ch, 1).map([](){});
		}
	
	protected:
		future<> sync() {
			return _stream.write_all(_buffer.data(), size()).map([this]() {
				clear();
			});
		}

		size_type space_left() const {
			return capacity() - size();
		}

		size_type size() const {
			return _write_offset;
		}

		size_type capacity() const {
			return _capacity;
		}

		size_type write_offset() const {
			return _write_offset;
		}

		char_type *pptr() const {
			return _buffer.data() + write_offset();
		}

		void clear() {
			_size = 0;
			_write_offset = 0;
		}

		void advance(size_type count) {
			_write_offset += count;
		}

		size_type write_now(const char_type* data, size_type count) {
			const size_type write_count = count < space_left() ? count : space_left();
			std::memcpy(pptr(), data, write_count);
			advance(write_count);
			return write_count;
		}
	};

	template <class CharT, class Traits = std::char_traits<CharT>>
	class combined_iostream : public basic_iostream<CharT, Traits> {
	protected:
		using stream_type = basic_ostream<CharT, Traits>;
		using _base = basic_iostream<CharT, Traits>;

	public:
		using char_type = CharT;
		using traits_type = Traits;
		using int_type = typename Traits::int_type;
		using pos_type = typename Traits::pos_type;
		using off_type = typename Traits::off_type;
		using istream_type = basic_istream<CharT, Traits>;
		using ostream_type = basic_ostream<CharT, Traits>;

	protected:
		std::shared_ptr<istream_type> _in;
		std::shared_ptr<ostream_type> _out;

	public:
		combined_iostream() = delete;
		combined_iostream(std::shared_ptr<istream_type> in, std::shared_ptr<ostream_type> out) : _in(in), _out(out) {}

		virtual future<std::size_t> read(char_type* dst, std::size_t count) {
			return _in->read(dst, count);
		}

		virtual future<std::size_t> write(const char_type* src, std::size_t count) {
			return _out->write(src, count);
		}

		virtual future<> flush() {
			return _out->flush();
		}
	};
}
#endif