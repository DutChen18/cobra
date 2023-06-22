#include "cobra/event_loop.hh"

#include "cobra/exception.hh"

extern "C" {
#include <sys/epoll.h>
}

namespace cobra {
	event_loop::~event_loop() {}

	epoll_event_loop::epoll_event_loop() : _epoll_fd(epoll_create(1)) {
		if (_epoll_fd.fd() == -1)
			throw errno_exception();
	}

	epoll_event_loop::epoll_event_loop(epoll_event_loop&& other) noexcept : _epoll_fd(std::move(other._epoll_fd)) {}

	void epoll_event_loop::poll() {

	}
}
