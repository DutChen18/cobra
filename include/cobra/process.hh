#ifndef COBRA_PROCESS_HH
#define COBRA_PROCESS_HH

#include "cobra/asio.hh"
#include "cobra/fd.hh"

#include <unordered_map>

namespace cobra {
	enum class fd_mode {
		none,
		pipe,
	};

	class process {
	private:
		int pid;
	public:
		fd_ostream in;
		fd_istream out;
		fd_istream err;

		process(int pid, int in_fd, int out_fd, int err_fd);
		process(const process& other) = delete;
		process(process&& other);
		~process();

		process& operator=(process other);

		void kill(int sig);
		future<int> wait();
	};

	class command {
	private:
		std::vector<std::string> argv;
		std::unordered_map<std::string, std::string> envp;
		fd_mode in_mode = fd_mode::none;
		fd_mode out_mode = fd_mode::none;
		fd_mode err_mode = fd_mode::none;
	public:
		command(std::initializer_list<std::string> args);

		command& set_in(fd_mode mode);
		command& set_out(fd_mode mode);
		command& set_err(fd_mode mode);
		command& insert_env(std::string key, std::string value);
		process spawn() const;
	};
}

#endif
