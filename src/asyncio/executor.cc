#include "cobra/asyncio/executor.hh"

namespace cobra {
	void executor::executor_event::operator()(event_handle<void>& handle) {
		_exec->schedule_event(handle);
	}

	executor::~executor() {}

	executor::event_type executor::schedule() {
		return {{ this }};
	}

	void sequential_executor::schedule_event(executor::event_type::handle_type& handle) {
		handle.set_value();
	}
}
