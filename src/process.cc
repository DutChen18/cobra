#include "cobra/process.hh"
#include "cobra/event_loop.hh"
#include "cobra/exception.hh"

#include <sys/wait.h>
#include <unistd.h>

namespace cobra {
	process::process(int pid, int read_fd, int write_fd) {
		this->pid = pid;
		this->read_fd = read_fd;
		this->write_fd = write_fd;
	}

	process::process(process&& other) {
		pid = other.pid;
		read_fd = other.read_fd;
		write_fd = other.write_fd;
		other.pid = -1;
		other.read_fd = -1;
		other.write_fd = -1;
	}

	process::~process() {
		if (pid >= 0) {
			std::cerr << "Unreaped child process pid " << pid << std::endl;
		}

		if (read_fd >= 0 && close(read_fd) == -1) {
			std::cerr << "Failed to properly close process read fd " << read_fd;
			std::cerr << ": " << std::strerror(errno) << std::endl;
		}

		if (write_fd >= 0 && close(write_fd) == -1) {
			std::cerr << "Failed to properly close process write fd " << write_fd;
			std::cerr << ": " << std::strerror(errno) << std::endl;
		}
	}

	process& process::operator=(process other) {
		std::swap(pid, other.pid);
		std::swap(read_fd, other.read_fd);
		std::swap(write_fd, other.write_fd);
		return *this;
	}

	future<std::size_t> process::read(char_type* dst, std::size_t count) {
		return cobra::future<std::size_t>([this, dst, count](context& ctx, future_func<std::size_t>& resolve) {
			ctx.on_ready(read_fd, listen_type::read, capture([this, dst, count](future_func<std::size_t>& resolve) {
				ssize_t nread = ::read(read_fd, dst, count);

				if (nread < 0) {
					resolve(err<std::size_t, future_error>(errno_exception()));
				} else {
					resolve(ok<std::size_t, future_error>(nread));
				}
			}, std::move(resolve)));
		});
	}

	future<std::size_t> process::write(const char_type* data, std::size_t count) {
		return future<std::size_t>([this, data, count](context& ctx, future_func<std::size_t>& resolve) {
			ctx.on_ready(write_fd, listen_type::write, capture([this, data, count](future_func<std::size_t>& resolve) {
				ssize_t nwritten = ::write(write_fd, data, count);

				if (nwritten < 0) {
					resolve(err<std::size_t, future_error>(errno_exception()));
				} else {
					resolve(ok<std::size_t, future_error>(nwritten));
				}
			}, std::move(resolve)));
		});
	}

	future<unit> process::flush() {
		return resolve(unit());
	}

	void process::kill(int sig) {
		::kill(pid, sig);
	}

	future<int> process::wait() {
		return future<int>([this](context& ctx, future_func<int>& resolve) {
			ctx.on_pid(pid, capture([this](future_func<int>& resolve) {
				int stat;
				pid_t rv = ::waitpid(pid, &stat, 0); // TODO: handle EINTR
				pid = -1;

				if (rv == -1) {
					resolve(err<int, future_error>(errno_exception()));
				} else {
					resolve(ok<int, future_error>(WEXITSTATUS(stat)));
				}
			}, std::move(resolve)));
		});
	}

	command::command(std::initializer_list<std::string> args) {
		this->argv = args;
	}

	process command::spawn() const {
		int write_fd[2];
		int read_fd[2];

		if (pipe(write_fd) == -1) {
			throw errno_exception();
		}

		if (pipe(read_fd) == -1) {
			int err = errno;
			close(write_fd[0]); // TODO: close wrapper to handle errors? (raii?)
			close(write_fd[1]);
			throw errno_exception(err);
		}

		int pid = fork();

		if (pid == -1) {
			int err = errno;
			close(write_fd[0]);
			close(write_fd[1]);
			close(read_fd[0]);
			close(read_fd[1]);
			throw errno_exception(err);
		} else if (pid == 0) {
			dup2(write_fd[0], STDIN_FILENO); // TODO: handle errors
			dup2(read_fd[1], STDOUT_FILENO);
			close(write_fd[0]);
			close(write_fd[1]);
			close(read_fd[0]);
			close(read_fd[1]);

			std::vector<const char*> argv;
			std::vector<std::string> envs;
			std::vector<const char*> envp;

			for (const auto& arg : this->argv)
				argv.push_back(arg.c_str());
			for (const auto& env : this->envp)
				envs.push_back(env.first + "=" + env.second);
			for (const auto& env : envs)
				envp.push_back(env.c_str());

			argv.push_back(NULL);
			envp.push_back(NULL);

			execve(argv[0], const_cast<char* const*>(argv.data()), const_cast<char* const*>(envp.data()));
			std::terminate();
		}

		close(write_fd[0]);
		close(read_fd[1]);

		return process(pid, read_fd[0], write_fd[1]);
	}
}
