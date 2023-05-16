#ifndef COBRA_FD_HH
#define COBRA_FD_HH

#include "cobra/asio.hh"

namespace cobra {
	class fd_wrapper {
	private:
		int fd;
	public:
		fd_wrapper(int fd = -1);
		fd_wrapper(const fd_wrapper& other) = delete;
		fd_wrapper(fd_wrapper&& other);
		~fd_wrapper();

		fd_wrapper& operator=(fd_wrapper other);

		int get() const;
		int leak();
	};
	
	class fd_istream : public basic_istream<char> {
	private:
		fd_wrapper fd;
	public:
		fd_istream(int fd = -1);

		future<std::size_t> read(char_type* dst, std::size_t count) override;
	};

	class fd_ostream : public basic_ostream<char> {
	private:
		fd_wrapper fd;
	public:
		fd_ostream(int fd = -1);

		future<std::size_t> write(const char_type* data, std::size_t count) override;
		future<unit> flush() override;
	};

	void pipe(fd_wrapper* fds);
}

#endif
