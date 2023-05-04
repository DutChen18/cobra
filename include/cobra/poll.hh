#ifndef COBRA_POLL_HH
#define COBRA_POLL_HH

#include <vector>
#include "cobra/task.hh"

namespace cobra {

	template <class Value>
	class poller {
	public:
		virtual ~poller();
		virtual void add(poll_target target, Value value) = 0;
		virtual void mod(int fd, fd_type type) = 0;
		virtual void remove(poll_target target) = 0;

		virtual const std::vector<std::pair<poll_target, Value>>& poll() = 0;
	};
}

#endif
