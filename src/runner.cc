#include "cobra/runner.hh"
#include "cobra/event_loop.hh"
#include "cobra/executor.hh"

namespace cobra {
	std::mutex& runner::get_mutex() const {
		return mutex;
	}

	std::condition_variable& runner::get_condition_variable() const {
		return condition_variable;
	}

	void runner::run(executor* exec, event_loop* loop) {
		std::unique_lock<std::mutex> guard(mutex);

		while (true) {
			while (loop != nullptr && !loop->done()) {
				guard.unlock();
				loop->poll();
				guard.lock();
			}

			if (exec == nullptr || exec->done()) {
				return;
			}

			condition_variable.wait(guard);
		}
	}
}
