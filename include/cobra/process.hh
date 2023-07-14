#ifndef COBRA_PROCESS_HH
#define COBRA_PROCESS_HH

#include "cobra/asyncio/event_loop.hh"
#include "cobra/asyncio/stream.hh"

namespace cobra {
	enum class process_stream_type {
		in,
		out,
		err,
	};

	enum class command_stream_mode {
		none,
		pipe,
	};

	template <process_stream_type Type>
	class process_istream : public istream_impl<process_istream<Type>>, public file {
	public:
		using typename istream_impl<process_istream<Type>>::char_type;

		task<std::size_t> read(char_type* data, std::size_t size);
	};

	template <process_stream_type Type>
	class process_ostream : public ostream_impl<process_ostream<Type>>, public file {
	public:
		using typename ostream_impl<process_ostream<Type>>::char_type;

		task<std::size_t> write(const char_type* data, std::size_t size);
		task<void> flush();
	};

	class process : public process_ostream<process_stream_type::in>, public process_istream<process_stream_type::out>, public process_istream<process_stream_type::err> {
		int _pid;
		event_loop* _loop;

	public:
		process(event_loop* loop, int pid, file&& in, file&& out, file&& err);
		process(const process& other) = delete;
		process(process&& other);
		~process();

		process& operator=(process other);

		event_loop* loop() const;

		process_ostream<process_stream_type::in>& in();
		process_istream<process_stream_type::out>& out();
		process_istream<process_stream_type::err>& err();

		task<int> wait();
	};

	class command {
		std::vector<std::string> _args;
		std::unordered_map<std::string, std::string> _env;
		command_stream_mode _in_mode = command_stream_mode::none;
		command_stream_mode _out_mode = command_stream_mode::none;
		command_stream_mode _err_mode = command_stream_mode::none;

	public:
		command(std::initializer_list<std::string> args);

		command& in(command_stream_mode mode);
		command& out(command_stream_mode mode);
		command& err(command_stream_mode mode);
		command& env(std::string key, std::string value);
		process spawn(event_loop* loop) const;
	};

	template <process_stream_type Type>
	task<std::size_t> process_istream<Type>::read(typename process_istream<Type>::char_type* data, std::size_t size) {
		co_await static_cast<process*>(this)->loop()->wait_read(*this);
		co_return check_return(::read(fd(), data, size));
	}

	template <process_stream_type Type>
	task<std::size_t> process_ostream<Type>::write(const typename process_ostream<Type>::char_type* data, std::size_t size) {
		co_await static_cast<process*>(this)->loop()->wait_write(*this);
		co_return check_return(::write(fd(), data, size));
	}

	template <process_stream_type Type>
	task<void> process_ostream<Type>::flush() {
		co_return;
	}
}

#endif
