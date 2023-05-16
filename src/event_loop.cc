#include "cobra/event_loop.hh"
#include "cobra/exception.hh"
#include "cobra/context.hh"

#include <functional>
#include <ios>
#include <mutex>
#include <stdexcept>
#include <iostream>
#include <cerrno>
#include <cstring>

extern "C" {
#include <sys/epoll.h>
#include <unistd.h>
}

namespace cobra {

	event_loop::event_loop(context* ctx) : ctx(ctx) {}
	event_loop::event_loop(event_loop&& other) noexcept : ctx(other.ctx) {}
	event_loop::~event_loop() {}
	//event_loop& event_loop::operator=(event_loop&&) noexcept { return *this; }

	void event_loop::lock() const {
		ctx->get_mutex().lock();
	}

	void event_loop::unlock() const {
		ctx->get_mutex().unlock();
	}

	epoll_event_loop::epoll_event_loop(context* ctx) : event_loop(ctx) {
		epoll_fd = epoll_create(1);

		if (epoll_fd == -1)
			throw errno_exception();
	}

	epoll_event_loop::~epoll_event_loop() {
		int rc = close(epoll_fd);
		if (rc == -1) {
			std::cerr << "Failed to properly close epoll_fd " << epoll_fd << ": "
				<< std::strerror(errno) << std::endl;
		}
	}

	void epoll_event_loop::poll() {
		std::vector<std::pair<int, listen_type>> events = poll(1);

		for (auto&& event : events) {
			optional<callback_type> callback = remove_callback(event);

			if (callback) {
				ctx->execute(std::move(*callback));
			}
		}
	}

	std::vector<epoll_event_loop::event_type> epoll_event_loop::poll(std::size_t count) {
		std::vector<epoll_event> events(count);
		int ready_count = 0;

		while (true) {
			int rc = epoll_wait(epoll_fd, events.data(), count, -1);

			if (rc == -1) {
				if (errno != EINTR)
					throw errno_exception();
			} else {
				ready_count = rc;
				break;
			}
		}

		std::vector<event_type> result;
		result.reserve(ready_count);

		for (int idx = 0; idx < ready_count; ++idx) {
			const epoll_event& event = events[idx];

			if (event.events & (EPOLLERR | EPOLLHUP)) {
				result.push_back(std::make_pair(event.data.fd, listen_type::read));
				result.push_back(std::make_pair(event.data.fd, listen_type::write));
			}

			if (event.events & EPOLLIN) {
				result.push_back(std::make_pair(event.data.fd, listen_type::read));
			}

			if (event.events & EPOLLOUT) {
				result.push_back(std::make_pair(event.data.fd, listen_type::write));
			}
		}
		return result;
	}

	bool epoll_event_loop::done() const {
		return read_callbacks.empty() && write_callbacks.empty();
	}

	int epoll_event_loop::get_events(listen_type type) const {
		switch (type) {
		case listen_type::write:
			return EPOLLOUT;
		case listen_type::read:
			return EPOLLIN;
		}
	}

	listen_type epoll_event_loop::get_type(int event) const {
		if (event == EPOLLIN) {
			return listen_type::read;
		} else if (event == EPOLLOUT) {
			return listen_type::write;
		}
		throw std::domain_error("Invalid event");
	}

	epoll_event_loop::callback_map& epoll_event_loop::get_map(listen_type type) {
		if (type == listen_type::read)
			return read_callbacks;
		return write_callbacks;
	}

	bool epoll_event_loop::has_callback(int fd) {
		return get_map(listen_type::read).count(fd) == 1 || get_map(listen_type::write).count(fd) == 1;
	}

	optional<epoll_event_loop::callback_type> epoll_event_loop::remove_callback(event_type event) {
		std::lock_guard<const epoll_event_loop> guard(*this);

		callback_map& map = get_map(event.second);

		auto it = map.find(event.first);
		if (it == map.end())
			return optional<callback_type>();

		optional<callback_type> result(std::move(it->second));
		map.erase(it);

		const bool has_one = has_callback(event.first);

		int rc = 0;

		if (has_one) {
			epoll_event epoll_event;
			epoll_event.events = event.second == listen_type::read ? EPOLLOUT : EPOLLIN;
			epoll_event.data.fd = event.first;
			rc = epoll_ctl(epoll_fd, EPOLL_CTL_MOD, event.first, &epoll_event);
		} else {
			rc = epoll_ctl(epoll_fd, EPOLL_CTL_DEL, event.first, nullptr);
		}

		if (rc == -1)
			throw errno_exception();
		return result;
	}

	void epoll_event_loop::on_ready(int fd, listen_type type, callback_type&& callback) {
		epoll_event event;
		event.events = get_events(type);
		event.data.fd = fd;

		callback_map& callbacks = get_map(type);

		std::lock_guard<const epoll_event_loop> guard(*this);

		const bool has_one = has_callback(fd);

		if (!callbacks.emplace(std::make_pair(fd, std::move(callback))).second) {
			throw std::invalid_argument("A callback for this operation was already registered for this fd");
		}
		int rc = 0;

		if (has_one) {
			event.events = EPOLLIN | EPOLLOUT;
			rc = epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &event);
		} else {
			rc = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event);
		}

		if (rc == -1) {
			callbacks.erase(fd);
			throw errno_exception();
		}

		ctx->get_condition_variable().notify_all(); //TODO move this to on_read_ready etc. so it's on by default
	}

	// TODO: implement this function
	void epoll_event_loop::on_pid(int pid, callback_type&& callback) {
		(void) pid;
		ctx->execute(std::move(callback));
	}
}
