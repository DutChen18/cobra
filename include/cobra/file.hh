#ifndef COBRA_FILE_HH
#define COBRA_FILE_HH

#include "cobra/asio.hh"

#include <fstream>

namespace cobra {
	template<class CharT, class Traits = std::char_traits<CharT>>
	class basic_fstream : public basic_iostream<CharT, Traits> {
	public:
		using char_type = CharT;
		using traits_type = Traits;
		using int_type = typename Traits::int_type;
		using pos_type = typename Traits::pos_type;
		using off_type = typename Traits::off_type;
	private:
		using stream_type = std::basic_fstream<CharT, Traits>;

		std::basic_fstream<CharT, Traits> stream;
	public:
		basic_fstream(const char* filename, typename stream_type::openmode mode) : stream(filename, mode) {
			stream.exceptions(stream_type::failbit | stream_type::badbit);
		}

		basic_fstream(const std::string& filename, typename stream_type::openmode mode) : stream(filename, mode) {
			stream.exceptions(stream_type::failbit | stream_type::badbit);
		}

		basic_fstream(basic_fstream&& other) : stream(std::move(other.stream)) {
		}

		basic_fstream(const basic_fstream&) = delete;

		future<std::size_t> read(char_type* dst, std::size_t count) override {
			stream.read(dst, count);
			return future<std::size_t>(stream.gcount());
		}

		future<std::size_t> write(const char_type* data, std::size_t count) override {
			stream.write(data, count);
			return future<std::size_t>(count);
		}

		future<> flush() override {
			stream.flush();
			return future<>();
		}
	};

	using fstream = basic_fstream<char>;
	using wfstream = basic_fstream<wchar_t>;
}

#endif
