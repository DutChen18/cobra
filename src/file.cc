#include "cobra/file.hh"

#include <utility>
#include <iostream>
#include <cstring>
#include <cerrno>

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

	void file::swap(file& other) noexcept {
		std::swap(_fd, other._fd);
	}

	file& file::operator=(file&& other) noexcept {
		if (this != &other) {
			swap(other);
		}
		return *this;
	}
}
