#ifndef COBRA_ASYNC_HH
#define COBRA_ASYNC_HH

#include <vector>
#include <cstddef>
#include "cobra/memory.hh"
#include "cobra/thread.hh"

namespace cobra {

	enum task_state {
		TASK_WAITING,
		TASK_DONE,
		TASK_ERROR
	};

	enum fd_type {
		FD_READ,
		FD_WRITE,
		FD_NONE,
	};

	struct poll_target {
		int fd;
		fd_type type;

		static poll_target none();
	};

	class task {
		atomic<task_state> state;

	public:
		task();

		virtual ~task();

		inline task_state get_state() const { return state; };

		bool poll();
	protected:
		virtual bool on_poll() = 0;

	public:
		virtual poll_target poll_fd() const;

		virtual const task* wait_task() const;
		virtual task* wait_task();

		bool ready() const;
	};

	class event_loop {
		std::vector<shared_ptr<task> > tasks;

	public:
		void schedule(shared_ptr<task> task);
		bool poll();
		void run();
	};

	class task_sequence : public task {
		std::vector<shared_ptr<task> > tasks;
		std::size_t progress;

	public:
		void schedule(moved<task> task);

		virtual int read_fd() const;
		virtual int write_fd() const;
		virtual int listen_fd() const;
		virtual task* wait_task() const;
		bool poll();
	};

	class task_group : public task {
		std::vector<shared_ptr<task> > tasks;
	public:

		virtual task* wait_task() const;
		bool poll();
	};
}
#endif
