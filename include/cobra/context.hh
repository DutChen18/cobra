#ifndef COBRA_CONTEXT_HH
#define COBRA_CONTEXT_HH

#include "cobra/function.hh"

#include <mutex>
#include <condition_variable>

namespace cobra {
	enum class listen_type;

	class executor;
	class event_loop;

	class context {
	private:
		mutable std::mutex mutex;
		mutable std::condition_variable condition_variable;
		std::unique_ptr<executor> exec;
		std::unique_ptr<event_loop> loop;
	public:
		context() = default;
		context(const context&) = delete;
		context(context&&) = delete;
		context& operator=(context) = delete;

		std::mutex& get_mutex() const;
		std::condition_variable& get_condition_variable() const;
		void set_executor(std::unique_ptr<executor>&& exec);
		void set_event_loop(std::unique_ptr<event_loop>&& loop);
		void execute(function<void>&& func) const;
		void on_ready(int fd, listen_type type, function<void>&& func) const;
		void on_pid(int pid, function<void>&& func) const;
		void run_until_complete() const;
	};

	std::unique_ptr<context> default_context();
}

#endif
