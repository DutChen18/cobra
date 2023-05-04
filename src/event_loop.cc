#include "cobra/event_loop.hh"
#include <algorithm>
#include <exception>
#include <initializer_list>
#include <memory>
#include <utility>
#include <iostream>
#include <stdexcept>

namespace cobra {

	namespace detail {

		task_scheduler::task_scheduler(std::initializer_list<std::shared_ptr<task>>) {}
		task_scheduler::task_scheduler(task_scheduler&&) noexcept { }
		task_scheduler::~task_scheduler() {}
	}

	void event_loop::deschedule(const task* t) {
		if (t == nullptr)
			return;
		tasks.erase(t);
		fd_poller->remove(t->target());
		dependencies.erase(t);
		deschedule(t->target().get_task());
	}

	void event_loop::schedule(const task* t) {
		tasks.insert(t);
		fd_poller->add(t->target(), t);

		if (t->target().get_task() != nullptr)
			dependencies[t->target().get_task()] = t;
	}

	bool event_loop::poll(std::shared_ptr<task> t) {
		const poll_target before = t->target();

		try {
			t->poll();
		} catch (const std::exception &ex) {
			t->set_exception(ex);
		} catch (...) {
			t->set_exception(std::runtime_error("An unknown exception was thrown in poll"));
		}

		switch (t->get_state()) {
		case task_state::error:
			std::cerr << "An exception occurred while executing a task" << std::endl;
			std::cerr << t->get_exception() << std::endl;
		case task_state::done:
			deschedule(t);
			//fd_poller->remove(t->target());
			return true;
		case task_state::wait:
			const poll_target after = t->target();

			if (before.get_fd() != after.get_fd()) {
				fd_poller->remove(before);
				fd_poller->add(after);
			} else if (before.get_type() != after.get_type()) {
				fd_poller->mod(after.get_fd(), after.get_type());
			} else if (before.get_task() != after.get_task()) {
				deschedule(before.get_task());
				schedule(after.get_task());
				dependencies[after.get_task()] = t;
			}

			return false;
		}
	}

	bool event_loop::poll() {
		std::vector<std::shared_ptr<task>> queue;

		const std::vector<std::pair<poll_target, std::shared_ptr<task>>>& fd_tasks = fd_poller->poll();
		//TODO poll all

		for (const auto& fd_task : fd_tasks) {
			if (poll(fd_task.second)) {
				auto next = dependencies.find(fd_task.second.get());
				if (next != dependencies.end())
					queue.push_back(next->second);
			}
		}

		for (std::size_t idx = 0; idx < queue.size(); ++idx) {
			if (poll(queue[idx])) {
				auto next = dependencies.find(queue[idx].get());
				if (next != dependencies.end())
					queue.push_back(next->second);
			}
		}
		return tasks.empty();
	}
}
