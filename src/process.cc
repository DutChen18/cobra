#include "cobra/process.hh"
#include "cobra/event_loop.hh"
#include "cobra/exception.hh"

#include <sys/wait.h>
#include <unistd.h>
#include <unordered_map>

namespace cobra {
	process::process(int pid, int in_fd, int out_fd, int err_fd) {
		this->pid = pid;
		this->in = in_fd;
		this->out = out_fd;
		this->err = err_fd;
	}

	process::process(process&& other) {
		pid = other.pid;
		other.pid = -1;
		in = std::move(other.in);
		out = std::move(other.out);
		err = std::move(other.err);
	}

	process::~process() {
		if (pid >= 0) {
			std::cerr << "Unreaped child process pid " << pid << std::endl;
		}
	}

	process& process::operator=(process other) {
		std::swap(pid, other.pid);
		std::swap(in, other.in);
		std::swap(out, other.out);
		std::swap(err, other.err);
		return *this;
	}

	void process::kill(int sig) {
		::kill(pid, sig);
	}

	future<int> process::wait() {
		return async_while<int>([this]() {
			return future<optional<int>>([this](context& ctx, future_func<optional<int>>& resolve) {
				ctx.on_pid(pid, capture([this](future_func<optional<int>>& resolve) {
					int stat;
					pid_t rv = ::waitpid(pid, &stat, 0);
					pid = -1;

					if (rv == -1 && errno == EINTR) {
						resolve(ok<optional<int>, future_error>(none<int>()));
					} else if (rv == -1) {
						resolve(::cobra::err<optional<int>, future_error>(errno_exception()));
					} else {
						resolve(ok<optional<int>, future_error>(some<int>(WEXITSTATUS(stat))));
					}
				}, std::move(resolve)));
			});
		});
	}

	command::command(std::initializer_list<std::string> args) {
		this->argv = args;
	}

	command& command::set_in(fd_mode mode) {
		in_mode = mode;
		return *this;
	}

	command& command::set_out(fd_mode mode) {
		out_mode = mode;
		return *this;
	}

	command& command::set_err(fd_mode mode) {
		err_mode = mode;
		return *this;
	}
	
	command& command::insert_env(std::string key, std::string value) {
		std::pair<std::unordered_map<std::string, std::string>::iterator, bool> result = envp.emplace(std::make_pair(std::move(key), std::move(value)));

		if (!result.second) {
			result.first->second = value;
		}

		return *this;
	}

	process command::spawn() const {
		fd_wrapper in_fd[2] = { -1, -1 };
		fd_wrapper out_fd[2] = { -1, -1 };
		fd_wrapper err_fd[2] = { -1, -1 };

		if (in_mode == fd_mode::pipe) {
			pipe(in_fd);
		}

		if (out_mode == fd_mode::pipe) {
			pipe(out_fd);
		}

		if (err_mode == fd_mode::pipe) {
			pipe(err_fd);
		}

		int pid = fork();

		if (pid == -1) {
			throw errno_exception();
		} else if (pid == 0) {
			if (in_mode == fd_mode::pipe) {
				dup2(in_fd[0].get(), STDIN_FILENO);
			}

			if (out_mode == fd_mode::pipe) {
				dup2(out_fd[1].get(), STDOUT_FILENO);
			}

			if (err_mode == fd_mode::pipe) {
				dup2(err_fd[1].get(), STDERR_FILENO);
			}

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

		return process(pid, in_fd[1].leak(), out_fd[0].leak(), err_fd[0].leak());
	}
}
