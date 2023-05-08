#ifndef COBRA_RUNNER_HH
#define COBRA_RUNNER_HH

#include <mutex>
#include <condition_variable>

namespace cobra {
	class executor;
	class event_loop;

	class runner {
	private:
		mutable std::mutex mutex;
		mutable std::condition_variable condition_variable;
	public:
		std::mutex& get_mutex() const;
		std::condition_variable& get_condition_variable() const;

		void run(executor* exec, event_loop* loop);
	};
}

#endif
