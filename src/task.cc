#include "cobra/task.hh"

namespace cobra {
	poll_target::poll_target() {
		type = fd_type::none;
	}

	poll_target::poll_target(int fd, fd_type type) {
		this->fd = fd;
		this->type = type;
	}

	poll_target::poll_target(const task& t) {
		this->t = &t;
		this->type = fd_type::task;
	}

	int poll_target::get_fd() const {
		return fd;
	}

	fd_type poll_target::get_type() const {
		return type;
	}

	task::~task() {
	}

	void task::poll() {
	}

	poll_target task::target() const {
		return poll_target();
	}

	task_state task::get_state() const {
		return state;
	}

	void task::set_state(task_state state) {
		this->state = state;
	}

	const std::exception* task::get_exception() const {
		return exception.get();
	}

	void task::set_exception(std::exception exception) {
		set_state(task_state::error);
		this->exception.reset(new std::exception(exception));
	}

	task_and_then_task::task_and_then_task(std::shared_ptr<task> before, std::function<std::shared_ptr<task>()> func) {
		this->before = before;
		this->func = func;
	}

	void task_and_then_task::poll() {
		if (!after) {
			switch (before->get_state()) {
			case task_state::done:
				after = func();
				break;
			case task_state::error:
				set_exception(*after->get_exception());
				break;
			case task_state::wait:
				break;
			}
		}

		if (after) {
			switch (after->get_state()) {
			case task_state::done:
				set_state(task_state::done);
				break;
			case task_state::error:
				set_exception(*after->get_exception());
				break;
			case task_state::wait:
				break;
			}
		}
	}

	poll_target task_and_then_task::target() const {
		if (after) {
			return poll_target(*after);
		} else {
			return poll_target(*before);
		}
	}
}
