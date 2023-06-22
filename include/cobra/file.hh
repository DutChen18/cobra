#ifndef COBRA_FD_HH
#define COBRA_FD_HH

namespace cobra {
	class file {
		int _fd;
	
	public:
		file() = delete;
		file(const file& other) = delete;

		file(int fd) noexcept;
		file(file&& other) noexcept;
		~file();

		file& operator=(const file& other) = delete;
		file& operator=(file&& other) noexcept;

		void swap(file& other) noexcept;

		inline int fd() const { return _fd; }
	};
}

#endif
