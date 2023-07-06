#include "cobra/process.hh"

#include <iostream>

extern "C" {
#include <fcntl.h>
}

namespace cobra {
	process::process(event_loop* loop, int pid, file&& in, file&& out, file&& err) : process_ostream<process_stream_type::in> { {}, std::move(in) }, process_istream<process_stream_type::out> { {}, std::move(out) }, process_istream<process_stream_type::err> { {}, std::move(err) }, _pid(pid), _loop(loop) {
	}

	process::process(process&& other) : process_ostream<process_stream_type::in>(std::move<process_ostream<process_stream_type::in>&>(other)), process_istream<process_stream_type::out>(std::move<process_istream<process_stream_type::out>&>(other)), process_istream<process_stream_type::err>(std::move<process_istream<process_stream_type::err>&>(other)), _pid(std::exchange(other._pid, 1)), _loop(other._loop) {
	}

	process::~process() {
		if (_pid != -1) {
			std::cerr << "Failed to properly wait on pid " << _pid << std::endl;
		}
	}

	process& process::operator=(process other) {
		std::swap<process_ostream<process_stream_type::in>>(*this, other);
		std::swap<process_istream<process_stream_type::out>>(*this, other);
		std::swap<process_istream<process_stream_type::err>>(*this, other);
		std::swap(_pid, other._pid);
		std::swap(_loop, other._loop);
		return *this;
	}

	event_loop* process::loop() const {
		return _loop;
	}

	process_ostream<process_stream_type::in>& process::in() {
		return *this;
	}

	process_istream<process_stream_type::out>& process::out() {
		return *this;
	}

	process_istream<process_stream_type::err>& process::err() {
		return *this;
	}
	
	command::command(std::initializer_list<std::string> args) : _args(args) {
	}

	command& command::in(command_stream_mode mode) {
		_in_mode = mode;
		return *this;
	}

	command& command::out(command_stream_mode mode) {
		_out_mode = mode;
		return *this;
	}

	command& command::err(command_stream_mode mode) {
		_err_mode = mode;
		return *this;
	}

	command& command::env(std::string key, std::string value) {
		_env.emplace(std::move(key), std::move(value));
		return *this;
	}

	static std::pair<file, file> pipe() {
		int fds[2];
		check_return(::pipe(fds));
		return { fds[0], fds[1] };
	}

	process command::spawn(event_loop* loop) const {
		std::pair<file, file> in = pipe();
		std::pair<file, file> out = pipe();
		std::pair<file, file> err = pipe();

		int pid = check_return(fork());

		if (pid == 0) {
			if (_in_mode == command_stream_mode::pipe)
				check_return(dup2(in.first.fd(), STDIN_FILENO));
			if (_out_mode == command_stream_mode::pipe)
				check_return(dup2(out.second.fd(), STDOUT_FILENO));
			if (_err_mode == command_stream_mode::pipe)
				check_return(dup2(err.second.fd(), STDERR_FILENO));

			std::vector<const char*> argv;
			std::vector<std::string> envs;
			std::vector<const char*> envp;

			for (const auto& arg : _args)
				argv.push_back(arg.c_str());
			for (const auto& env : _env)
				envs.push_back(env.first + "=" + env.second);
			for (const auto& env : envs)
				envp.push_back(env.c_str());

			argv.push_back(nullptr);
			envp.push_back(nullptr);

			execve(argv[0], const_cast<char* const*>(argv.data()), const_cast<char* const*>(envp.data()));
			std::terminate();
		}

		check_return(fcntl(in.second.fd(), F_SETFL, O_NONBLOCK));
		check_return(fcntl(out.first.fd(), F_SETFL, O_NONBLOCK));
		check_return(fcntl(err.first.fd(), F_SETFL, O_NONBLOCK));

		return process(loop, pid, std::move(in.second), std::move(out.first), std::move(err.first));
	}
}
