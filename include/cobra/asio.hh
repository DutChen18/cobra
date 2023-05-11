#ifndef COBRA_ASIO_HH
#define COBRA_ASIO_HH

#include "cobra/function.hh"
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

extern "C" {
#include <signal.h>
}

namespace cobra {

	template <class CharT, class Traits = std::char_traits<CharT>>
	class basic_ios {
	protected:
		basic_ios() = default;
		virtual ~basic_ios() = default;

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

		virtual future<optional<int_type>> get() {
			std::shared_ptr<char_type> tmp = std::make_shared<char_type>(traits_type::to_char_type(traits_type::eof()));
			return read(tmp.get(), 1).template and_then<optional<int_type>>([tmp](std::size_t nread) {
				if (nread == 0)
					return resolve(none<int_type>());
				return resolve(some<int_type>(traits_type::to_int_type(*tmp)));
			});
		}

		virtual future<std::size_t> ignore(const std::size_t count, const int_type delim = traits_type::eof()) {
			std::size_t num_ignored = 0;
			return async_while<std::size_t>([this, count, delim, num_ignored]() mutable {
				if (num_ignored < count) {
					return get().template and_then<optional<std::size_t>>([delim, &num_ignored](optional<int_type> ch) {
						num_ignored += 1;
						if (!ch || ch == delim)
							return resolve(some<std::size_t>(num_ignored));
						return resolve(none<std::size_t>());
					});
				} else {
					return resolve(some<std::size_t>(num_ignored));
				}
			});
		}
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
		virtual future<unit> flush() = 0;

		virtual future<std::size_t> write_all(const char_type* data, std::size_t count) {
			std::size_t progress = 0;
			return async_while<std::size_t>(capture([this, data, count](std::size_t& progress) {
				return write(data + progress, count - progress).template map<optional<std::size_t>>([&progress, count](result<std::size_t, future_error> nwritten) {
					if (!nwritten) {
						return reject<optional<std::size_t>>(*nwritten.err());
					}
					progress += *nwritten;
					if (*nwritten == 0 || progress == count)
						return resolve(some<std::size_t>(progress));
					return resolve(none<std::size_t>());
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
	class basic_buffered_istream : public virtual basic_istream<CharT, Traits> {
        public:
		using _base = basic_istream<CharT, Traits>;
		using char_type = typename _base::char_type;
		using traits_type = typename _base::traits_type;
		using int_type = typename _base::int_type;
		using pos_type = typename _base::pos_type;
		using off_type = typename _base::off_type;
		using allocator_type = Allocator;
		using buffer_type = std::vector<char_type, allocator_type>;
		using size_type = typename buffer_type::size_type;

	protected:
		using stream_type = _base;
		
		std::unique_ptr<stream_type> _stream;
		size_type _capacity = 0; //not needed
		buffer_type _buffer;
		size_type _size = 0;
		size_type _read_offset = 0;


	public:
		basic_buffered_istream() = delete;
		basic_buffered_istream(const basic_buffered_istream&) = delete;

		template<class Stream>
		basic_buffered_istream(Stream&& stream, size_type capacity = 1024) noexcept : _stream(make_unique<Stream>(std::move(stream))), _capacity(capacity), _buffer(capacity) {
		}

		basic_buffered_istream(basic_buffered_istream&& other) noexcept : basic_buffered_istream(std::move(other._stream)) {
			_capacity = other._capacity;
			other._capacity = 0;
			std::swap(_buffer, other._buffer);
			std::swap(_size, other._size);
			std::swap(_read_offset, other._read_offset);
		}

		basic_buffered_istream& operator=(const basic_buffered_istream&) = delete;

		size_type in_avail() const {
			return egptr() - gptr();
		}

		virtual future<size_type> read(char_type* dst, size_type count) override {
			return fill_buf().template and_then<size_type>([this, dst, count](size_type) {
				return resolve(read_now(dst, count));
			});
		}

		virtual future<optional<int_type>> get() override {
			return fill_buf().template and_then<optional<int_type>>([this](size_type) {
				return resolve(get_now());
			});
		}

		virtual future<optional<int_type>> peek() {
			return fill_buf().template and_then<optional<int_type>>([this](size_type) {
				return resolve(peek_now());
			});
		}

		char_type *gptr() const {
			return const_cast<char_type*>(_buffer.data() + read_offset());
		}

		char_type *egptr() const {
			return const_cast<char_type*>(_buffer.data() + size());
		}

	protected:
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
				_size = 0;
				return _stream->read(_buffer.data(), capacity()).template and_then<size_type>([this](size_type nread) {
					_size = nread;
					return resolve(std::move(nread));
				});
			}
			return resolve<size_type>(in_avail());
		}

		size_type read_now(char_type* dst, size_type count) {
			size_type capacity = count < in_avail() ? count : in_avail();
			std::memcpy(dst, gptr(), capacity);
			consume(capacity);
			return capacity;
		}

		optional<int_type> peek_now() {
			if (empty())
				return none<int_type>();
			return some<int_type>(traits_type::to_int_type(*gptr()));
		}

		optional<int_type> get_now() {
			const optional<int_type> result = peek_now();
			if (result)
				consume(1);
			return result;
		}
	};

	template <class CharT, class Traits = std::char_traits<CharT>, class Allocator = std::allocator<CharT>>
	class basic_buffered_ostream : public virtual basic_ostream<CharT, Traits> {
        public:
		using _base = basic_ostream<CharT, Traits>;
		using char_type = typename _base::char_Type;
		using traits_type = typename _base::traits_type;
		using int_type = typename _base::int_type;
		using pos_type = typename _base::pos_type;
		using off_type = typename _base::off_type;
		using allocator_type = Allocator;
		using buffer_type = std::vector<char_type, allocator_type>;
		using size_type = typename buffer_type::size_type;

	protected:
		using stream_type = _base;

		std::unique_ptr<stream_type> _stream;
		size_type _capacity = 0;
		buffer_type _buffer;
		size_type _size = 0;
		size_type _write_offset = 0;


	public:
		basic_buffered_ostream() = delete;

		template<class Stream>
		basic_buffered_ostream(Stream&& stream, size_type capacity = 1024) noexcept : _stream(make_unique<Stream>(std::move(stream))), _capacity(capacity), _buffer(capacity) {}

		basic_buffered_ostream(basic_buffered_ostream&& other) noexcept : basic_buffered_ostream(std::move(other._stream)) {
			_capacity = other._capacity;
			other._capacity = 0;
			std::swap(_buffer, other._buffer);
			std::swap(_size, other._size);
			std::swap(_write_offset, other._write_offset);
		}

		virtual future<size_type> write(const char_type* data, size_type count) override {
			if (count > capacity()) {
				return sync().template and_then<size_type>([this, data, count]() {
					return _stream->write_all(data, count);
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

		virtual future<unit> flush() override {
			return sync().template and_then<unit>([this]() {
				return _stream->flush();
			});
		}

		virtual future<unit> put(char_type ch) {
			return write(&ch, 1).map([](){});
		}
	
	protected:
		future<unit> sync() {
			return _stream->write_all(_buffer.data(), size()).map([this]() {
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
			return const_cast<char_type*>(_buffer.data() + write_offset());
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

		virtual future<unit> flush() {
			return _out->flush();
		}
	};

	using buffered_istream = basic_buffered_istream<char>;
	using istream = basic_istream<char>;
}
#endif
