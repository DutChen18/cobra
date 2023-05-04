#ifndef COBRA_EVENT_LOOP_HH
#define COBRA_EVENT_LOOP_HH

#include <initializer_list>
#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>

#include "cobra/task.hh"
#include "cobra/poll.hh"

namespace cobra {

	namespace detail {
		class task_scheduler { //TODO rename this to something better. task_bundle or something like that
		public:
			task_scheduler() = default;
			task_scheduler(std::initializer_list<std::shared_ptr<task>> tasks);
			task_scheduler(const task_scheduler &) = delete;
			task_scheduler(task_scheduler&& other) noexcept;
			virtual ~task_scheduler();

			task_scheduler& operator=(const task_scheduler &) = delete;
			virtual task_scheduler& operator=(task_scheduler&& other) noexcept = 0;

			virtual void schedule(std::shared_ptr<task> task) = 0;
		};
	}

	class event_loop : public detail::task_scheduler {
	protected:
		//TODO list to keep track of all scheduled tasks
		std::unordered_set<const task*> tasks;
		std::unordered_map<const task*, std::shared_ptr<task>> dependencies;
		std::unique_ptr<poller<std::shared_ptr<task>>> fd_poller;

	public:
		void run();

		using detail::task_scheduler::schedule;

	protected:
		void deschedule(const task* t);
		void deschedule(const std::shared_ptr<task> t);
		void schedule(const task* task);

		bool poll(std::shared_ptr<task> task);
		bool poll();
	};

	class task_sequence : public task, public detail::task_scheduler {
	public:
		virtual poll_target target() const;
		virtual const task* wait_task() const;

		virtual bool poll();
	};

	class task_group : public task, public detail::task_scheduler {
	public:
		virtual poll_target target() const;
		virtual const task* wait_task() const;

		virtual bool poll();
	};
}
#endif
