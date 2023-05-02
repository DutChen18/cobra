#include <iostream>
#include <exception>
#include "cobra/async.hh"

extern "C" {
#include <poll.h>
}

namespace cobra {

	task::task() : state(TASK_WAITING) {}
	task::~task() {}

	bool task::poll() {
		try {
			return on_poll();
		} catch (const std::exception &ex) {
			std::cerr << ex.what() << std::endl;
		} catch (...) {
			state = TASK_ERROR;
			std::cerr << "Unknown exception ocurred" << std::endl;
		}
		return true;
	}

	poll_target task::poll_fd() const {
		return poll_target::none();
	}

	const task* task::wait_task() const {
		return NULL;
	}

	task* task::wait_task() {
		return const_cast<task*>(static_cast<const task *>(this)->wait_task());
	}

	bool task::ready() const {
		if (!wait_task())
			return true;
		return wait_task()->get_state() == TASK_DONE;
	}

	void event_loop::schedule(shared_ptr<task> t) {
		tasks.push_back(t);
	}

	bool event_loop::poll() {
		std::vector<pollfd> fds;

		//TODO poll fds

		for (std::size_t idx = 0; idx < fds.size(); ++idx) {
			const short revents = fds[idx].revents;
			const int fd = fds[idx].fd;

			if (revents & POLLERR) {
				std::cerr << "An unknown error ocurred when polling fd " << fd << std::endl;
				continue;
			}

			for (std::size_t task_idx = 0; task_idx < tasks.size(); ++task_idx) {
				if (fd == tasks[idx]->poll_fd().fd) {
				}
			}
		}
		return false;
	}
}

