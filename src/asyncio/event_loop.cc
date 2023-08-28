#include "cobra/asyncio/event_loop.hh"

#include "cobra/exception.hh"

#include <atomic>
#include <chrono>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <tuple>

#ifdef COBRA_LINUX
extern "C" {
#include <sys/epoll.h>
}
#endif

namespace cobra {

	poll_type operator!(poll_type type) {
		return type == poll_type::read ? poll_type::write : poll_type::read;
	}

	event_loop::~event_loop() {}

	event_loop::event_type event_loop::wait_read(const file& fd, std::optional<std::chrono::milliseconds> timeout) {
		return wait_ready(poll_type::read, fd, timeout);
	}

	event_loop::event_type event_loop::wait_write(const file& fd, std::optional<std::chrono::milliseconds> timeout) {
		return wait_ready(poll_type::write, fd, timeout);
	}

	event_loop::event_type event_loop::wait_ready(poll_type type, const file& fd,
												  std::optional<std::chrono::milliseconds> timeout) {
		return event_loop::event_loop_event{*this, std::make_pair(fd.fd(), type), timeout};
	}

	task<int> event_loop::wait_pid(int pid, std::optional<std::chrono::milliseconds> timeout) {
		int result;
		
		while (true) {
			int ret = waitpid(pid, &result, 0); // TODO: very very very bad
			
			if (ret >= 0) {
				break;
			} else if (errno != EINTR) {
				throw errno_exception();
			}
		}

		(void) timeout;
		co_return WEXITSTATUS(result);
	}

	void event_loop::event_loop_event::operator()(event_handle<void>& handle) {
		_loop.get().schedule_event(_event, _timeout, handle);
	}

#ifdef COBRA_LINUX
	epoll_event_loop::epoll_event_loop(executor& exec) : _epoll_fd(epoll_create(1)), _exec(exec) {
		if (_epoll_fd.fd() == -1)
			throw errno_exception();
	}

	epoll_event_loop::epoll_event_loop(epoll_event_loop&& other) noexcept
		: _epoll_fd(std::move(other._epoll_fd)), _exec(other._exec), _write_events(std::move(other._write_events)),
		  _read_events(std::move(other._read_events)) {}

	void epoll_event_loop::schedule_event(event_pair event, std::optional<std::chrono::milliseconds> timeout,
										  event_handle<void>& handle) {
		std::optional<clock::duration> converted;

		if (timeout)
			converted = clock::duration(*timeout);
		add_event(event, converted, handle);
	}

	std::vector<epoll_event> epoll_event_loop::epoll(std::size_t count, std::optional<clock::duration> timeout) {
		std::vector<epoll_event> events(count);

		std::optional<time_point> timeout_point;

		auto now = clock::now();

		if (timeout.has_value())
			timeout_point = now + timeout.value();

		while (true) {
			auto epoll_timeout = clock::duration(std::chrono::milliseconds(-1));
			if (timeout_point.has_value())
				epoll_timeout = timeout_point.value() - now;

			int rc = epoll_wait(_epoll_fd.fd(), events.data(), count, epoll_timeout.count());

			if (rc == -1) {
				if (errno == EINTR) {
					now = clock::now();
					if (clock::now() >= timeout_point.value_or(time_point::max())) {
						return std::vector<epoll_event>();
					} else {
						continue;
					}
				} else {
					throw errno_exception();
				}
			} else {
				events.resize(rc);
				return events;
			}
		}
	}

	generator<std::pair<int, poll_type>> epoll_event_loop::convert(epoll_event event) {
		int fd = event.data.fd;

		if (event.events & (EPOLLERR | EPOLLHUP)) {
			co_yield {fd, poll_type::read};
			co_yield {fd, poll_type::write};
		} else {
			if (event.events & EPOLLIN) {
				co_yield {fd, poll_type::read};
			}

			if (event.events & EPOLLOUT) {
				co_yield {fd, poll_type::write};
			}
		}

		co_return;
	}

	std::vector<epoll_event_loop::event_pair> epoll_event_loop::poll(std::size_t count,
																	 std::optional<clock::duration> timeout) {
		std::vector<epoll_event> events = epoll(count, timeout);
		std::vector<event_pair> result;
		result.reserve(events.size());

		for (auto&& event : events) {
			for (auto&& conv : convert(event)) {
				result.push_back(conv);
			}
		}
		return result;
	}

	void epoll_event_loop::poll() {
		std::optional<clock::duration> timeout;

		auto now = clock::now();

		_mutex.lock();
		remove_before(now);

		std::optional<time_point> timeout_point = get_timeout();
		if (timeout_point)
			timeout = *timeout_point - now;
		_mutex.unlock();

		event_list events = poll(10, timeout);

		for (auto&& event : events) {
			auto future = remove_event(event);

			if (future) {
				auto handle = future.value();
				_exec.get().schedule([handle]() {
					handle.get().set_value();
				});
			}
		}
	}

	bool epoll_event_loop::has_events() const {
		std::lock_guard guard(_mutex);
		return !_write_events.empty() || !_read_events.empty() || _exec.get().has_jobs();
	}

	void epoll_event_loop::remove_before(std::unordered_map<int, timed_future>& map, time_point point) {
		for (auto it = map.begin(); it != map.end();) {
			if (it->second.timeout.value_or(time_point::max()) <= point) {
				auto event = map.find(it->first);

				event->second.future.get().set_exception(std::make_exception_ptr(timeout_exception()));

				map.erase(it++);
			} else {
				++it;
			}
		}
	}

	void epoll_event_loop::remove_before(time_point point) {
		remove_before(get_map(poll_type::read), point);
		remove_before(get_map(poll_type::write), point);
	}

	std::optional<epoll_event_loop::time_point>
	epoll_event_loop::get_timeout(const std::unordered_map<int, timed_future>& map) {
		std::optional<time_point> timeout;

		for (auto&& [fd, future] : map) {
			if (future.timeout) {
				if (timeout) {
					if (future.timeout < timeout)
						timeout = future.timeout;
				} else {
					timeout = future.timeout;
				}
			}
		}
		return timeout;
	}

	std::optional<epoll_event_loop::time_point> epoll_event_loop::get_timeout() {
		std::optional<time_point> write = get_timeout(get_map(poll_type::read));
		std::optional<time_point> read = get_timeout(get_map(poll_type::write));

		if (write && read) {
			return std::min(read, write);
		} else if (write) {
			return write;
		} else {
			return read;
		}
	}

	void epoll_event_loop::add_event(event_pair event, std::optional<clock::duration> timeout, future_type& future) {
		std::optional<time_point> timeout_point;

		if (timeout)
			timeout_point = clock::now() + *timeout;

		std::lock_guard<std::mutex> lock(_mutex);

		bool is_mod = get_map(!event.second).contains(event.first);
		if (!get_map(event.second).emplace(std::make_pair(event.first, timed_future{future, timeout_point})).second) {
			throw std::invalid_argument("A future already exists for this event");
		}

		epoll_event epoll_event;
		epoll_event.events = event_to_epoll(event.second);
		epoll_event.data.fd = event.first;

		int rc = 0;
		if (is_mod) {
			epoll_event.events |= event_to_epoll(!event.second);
			rc = epoll_ctl(_epoll_fd.fd(), EPOLL_CTL_MOD, epoll_event.data.fd, &epoll_event);
		} else {
			rc = epoll_ctl(_epoll_fd.fd(), EPOLL_CTL_ADD, epoll_event.data.fd, &epoll_event);
		}

		if (rc == -1) {
			get_map(event.second).erase(event.first);
			throw errno_exception();
		}
	}

	std::optional<std::reference_wrapper<epoll_event_loop::future_type>>
	epoll_event_loop::remove_event(event_pair event) {
		std::lock_guard<std::mutex> lock(_mutex);

		auto it = get_map(event.second).find(event.first);
		bool is_mod = get_map(!event.second).contains(event.first);

		if (it == get_map(event.second).end()) {
			return std::nullopt;
		}

		auto result = it->second.future;

		get_map(event.second).erase(it);

		int rc = 0;
		if (is_mod) {
			epoll_event epoll_event;
			epoll_event.events = event_to_epoll(!event.second);
			epoll_event.data.fd = event.first;
			rc = epoll_ctl(_epoll_fd.fd(), EPOLL_CTL_MOD, event.first, &epoll_event);
		} else {
			rc = epoll_ctl(_epoll_fd.fd(), EPOLL_CTL_DEL, event.first, nullptr);
		}

		if (rc == -1)
			throw errno_exception();
		return result;
	}
#endif

#ifdef COBRA_MACOS
	kqueue_event_loop::kqueue_event_loop(executor& exec) : _kqueue_fd(kqueue()), _exec(exec) {
		if (_kqueue_fd.fd() == -1)
			throw errno_exception();
	}

	void kqueue_event_loop::schedule_event(event_pair event, std::optional<std::chrono::milliseconds> timeout,
										  event_handle<void>& handle) {
		std::optional<time_point> timeout_point;

		if (timeout)
			timeout_point = clock::now() + *timeout;

		std::lock_guard guard(_mutex);
		struct kevent change;

		(void)timeout;
		EV_SET(change, event.first, EV_ADD | EV_ONESHOT, event.second == poll_type::read ? EVFILT_READ : EVFILT_WRITE, 0, 0, NULL);
		if (kevent(_kqueue_fd.fd(), &change, 1, NULL, 0, NULL) == -1)
			throw errno_exception();
		get_map(event.second)[event.first] = timed_future{handle, timeout_point};
	}

	std::optional<std::reference_wrapper<future_type>> kqueue_event_loop::remove_event(event_pair event) {
		std::lock_guard guard(_mtx);
		auto& map = get_map(event.second);
		auto it = map.find(event.first);

		if (it == map.end())
			return std::nullopt;

		auto result = it->second.future;
		map.erase(it);
		return result;
	}

	void kqueue_event_loop::poll() {
		struct kevent events[10];

		int ret = kevent(_kqueue_fd.fd(), NULL, 0, events, sizeof(events) / sizeof(events[0]), NULL);
		if (ret == -1)
			throw errno_exception();

		for (int i = 0; i < ret; ++i) {
			poll_type type = events[i].ffilter & EVFILT_READ ? poll_type::read : poll_type::write;
			auto fut = remove_event({evetns[i].ident, type});
			if (fut) {
				auto handle = future.value();
				_exec.get().schedule([handle]() {
					handle.get().set_value();
				});
			}
		}

	}
#endif
} // namespace cobra
