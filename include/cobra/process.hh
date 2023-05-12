#ifndef COBRA_PROCESS_HH
#define COBRA_PROCESS_HH

#include "cobra/asio.hh"

#include <unordered_map>

namespace cobra {
	class process : public basic_iostream<char> {
	private:
		int pid;
		int read_fd;
		int write_fd;
	public:
		process(int pid, int read_fd, int write_fd);
		process(const process& other) = delete;
		process(process&& other);
		~process();

		process& operator=(process other);

		future<std::size_t> read(char_type* dst, std::size_t count) override;
		future<std::size_t> write(const char_type* data, std::size_t count) override;
		future<unit> flush() override;

		void kill(int sig);
		future<int> wait();
	};

	class command {
	private:
		std::vector<std::string> argv;
		std::unordered_map<std::string, std::string> envp;
	public:
		command(std::initializer_list<std::string> args);

		process spawn() const;
	};
}

#endif
