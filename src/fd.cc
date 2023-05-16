#include "cobra/fd.hh"
#include "cobra/event_loop.hh"
#include "cobra/exception.hh"

#include <sys/wait.h>
#include <unistd.h>

namespace cobra {
	fd_wrapper::fd_wrapper(int fd) {
		this->fd = fd;
	}

	fd_wrapper::fd_wrapper(fd_wrapper&& other) {
		fd = other.leak();
	}

	fd_wrapper::~fd_wrapper() {
		if (fd >= 0 && close(fd) == -1) {
			std::cerr << "Failed to properly close fd " << fd;
			std::cerr << ": " << std::strerror(errno) << std::endl;
		}
	}

	fd_wrapper& fd_wrapper::operator=(fd_wrapper other) {
		std::swap(fd, other.fd);
		return *this;
	}

	int fd_wrapper::get() const {
		return fd;
	}

	int fd_wrapper::leak() {
		int result = fd;
		fd = -1;
		return result;
	}

	fd_istream::fd_istream(int fd) {
		this->fd = fd;
	}

	future<std::size_t> fd_istream::read(char_type* dst, std::size_t count) {
		return cobra::future<std::size_t>([this, dst, count](context& ctx, future_func<std::size_t>& resolve) {
			ctx.on_ready(fd.get(), listen_type::read, capture([this, dst, count](future_func<std::size_t>& resolve) {
				ssize_t nread = ::read(fd.get(), dst, count);

				if (nread < 0) {
					resolve(err<std::size_t, future_error>(errno_exception()));
				} else {
					resolve(ok<std::size_t, future_error>(nread));
				}
			}, std::move(resolve)));
		});
	}
	
	fd_ostream::fd_ostream(int fd) {
		this->fd = fd;
	}

	future<std::size_t> fd_ostream::write(const char_type* data, std::size_t count) {
		return future<std::size_t>([this, data, count](context& ctx, future_func<std::size_t>& resolve) {
			ctx.on_ready(fd.get(), listen_type::write, capture([this, data, count](future_func<std::size_t>& resolve) {
				ssize_t nwritten = ::write(fd.get(), data, count);

				if (nwritten < 0) {
					resolve(err<std::size_t, future_error>(errno_exception()));
				} else {
					resolve(ok<std::size_t, future_error>(nwritten));
				}
			}, std::move(resolve)));
		});
	}

	future<unit> fd_ostream::flush() {
		return resolve(unit());
	}
	
	void pipe(fd_wrapper* fds) {
		int tmp[2];

		if (::pipe(tmp) == -1) {
			throw errno_exception();
		}

		fds[0] = fd_wrapper(tmp[0]);
		fds[1] = fd_wrapper(tmp[1]);
	}
}
