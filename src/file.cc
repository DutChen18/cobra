#include "cobra/file.hh"

#include <cerrno>
#include <cstring>
#include <iostream>
#include <utility>

extern "C" {
#include <unistd.h>
}

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
} // namespace cobra
