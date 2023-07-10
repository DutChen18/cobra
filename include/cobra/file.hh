#ifndef COBRA_FILE_HH
#define COBRA_FILE_HH

extern "C" {
#include "unistd.h"
}

namespace cobra {
	class file {
		int _fd;

	public:
		file() = delete;
		file(const file& other) = delete;

		file(int fd) noexcept;
		file(file&& other) noexcept;
		~file();

		file& operator=(file other) noexcept;

		inline int fd() const {
			return _fd;
		}

		void close();
	};
	
	ssize_t check_return(ssize_t ret);
} // namespace cobra

#endif
