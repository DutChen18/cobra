#include "cobra/file.hh"
#include "cobra/exception.hh"

#include <cerrno>
#include <cstring>
#include <iostream>
#include <utility>

namespace cobra {
	file::file(int fd) noexcept : _fd(fd) {}

	file::file(file&& other) noexcept : _fd(std::exchange(other._fd, -1)) {}

	file::~file() {
		if (_fd != -1) {
			int rc = close(_fd);

			if (rc == -1)
				std::cerr << "Failed to properly close fd " << _fd << ": " << std::strerror(errno) << std::endl;
		}
	}

	file& file::operator=(file other) noexcept {
		std::swap(_fd, other._fd);
		return *this;
	}
	
	ssize_t check_return(ssize_t ret) {
		if (ret < 0 && errno != EINPROGRESS) {
			throw errno_exception();
		} else {
			return ret;
		}
	}
} // namespace cobra
