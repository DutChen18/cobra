#include "cobra/context.hh"
#include "cobra/event_loop.hh"
#include "cobra/executor.hh"

namespace cobra {
	std::mutex& context::get_mutex() const {
		return mutex;
	}

	std::condition_variable& context::get_condition_variable() const {
		return condition_variable;
	}

	void context::set_executor(std::unique_ptr<executor>&& exec) {
		this->exec = std::move(exec);
	}

	void context::set_event_loop(std::unique_ptr<event_loop>&& loop) {
		this->loop = std::move(loop);
	}

	void context::execute(function<void>&& func) const {
		if (exec) {
			exec->exec(std::move(func));
		} else {
			func();
		}
	}

	void context::on_ready(int fd, listen_type type, function<void>&& func) const {
		loop->on_ready(fd, type, std::move(func));
	}
	
	void context::on_pid(int pid, function<void>&& func) const {
		loop->on_pid(pid, std::move(func));
	}

	void context::run_until_complete() const {
		std::unique_lock<std::mutex> guard(mutex);

		while (true) {
			while (loop && !loop->done()) {
				guard.unlock();
				loop->poll();
				guard.lock();
			}

			if (!exec || exec->done()) {
				return;
			}

			condition_variable.wait(guard);
		}
	}

	std::unique_ptr<context> default_context() {
		std::unique_ptr<context> ctx = make_unique<context>();
		ctx->set_executor(make_unique<thread_pool_executor>(ctx.get(), 8));
		// ctx->set_executor(make_unique<sequential_executor>(ctx.get()));
		ctx->set_event_loop(make_unique<epoll_event_loop>(ctx.get()));
		return ctx;
	}
}
