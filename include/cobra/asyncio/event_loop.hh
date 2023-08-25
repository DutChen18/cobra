#ifndef COBRA_ASYNCIO_EVENT_LOOP_HH
#define COBRA_ASYNCIO_EVENT_LOOP_HH

#include "cobra/asyncio/event.hh"
#include "cobra/asyncio/executor.hh"
#include "cobra/asyncio/generator.hh"
#include "cobra/asyncio/task.hh"
#include "cobra/file.hh"

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>

#ifdef COBRA_LINUX
extern "C" {
#include <sys/epoll.h>
#include <sys/wait.h>
}
#endif

namespace cobra {

	enum class poll_type {
		read,
		write,
	};

	poll_type operator!(poll_type type);

	class event_loop {
	public:
		using event_pair = std::pair<int, poll_type>;

	protected:
		struct event_loop_event {
			std::reference_wrapper<event_loop> _loop;
			event_pair _event;
			std::optional<std::chrono::milliseconds> _timeout;

			void operator()(event_handle<void>& handle);
		};

	public:
		using event_type = event<void, event_loop_event>;

		virtual ~event_loop();

		event_type wait_ready(poll_type type, const file& fd,
							  std::optional<std::chrono::milliseconds> timeout = std::nullopt);
		event_type wait_read(const file& fd, std::optional<std::chrono::milliseconds> timeout = std::nullopt);
		event_type wait_write(const file& fd, std::optional<std::chrono::milliseconds> timeout = std::nullopt);

		task<int> wait_pid(int pid, std::optional<std::chrono::milliseconds> timeout = std::nullopt);

		virtual void poll() = 0;
		virtual bool has_events() const = 0;

	private:
		virtual void schedule_event(event_pair event, std::optional<std::chrono::milliseconds> timeout,
									event_type::handle_type& handle) = 0;
	};

#ifdef COBRA_LINUX
	class epoll_event_loop : public event_loop {
	public:
		using clock = std::chrono::steady_clock;
		using future_type = event_handle<void>;
		using time_point = clock::time_point;
		using event_pair = std::pair<int, poll_type>;

	private:
		file _epoll_fd;
		mutable std::mutex _mutex;
		std::reference_wrapper<executor> _exec;

		struct timed_future {
			std::reference_wrapper<future_type> future;
			std::optional<time_point> timeout;
		};

		std::unordered_map<int, timed_future> _write_events;
		std::unordered_map<int, timed_future> _read_events;

		using event_list = std::vector<event_pair>;

	public:
		epoll_event_loop(executor& exec);
		epoll_event_loop(const epoll_event_loop& other) = delete;
		epoll_event_loop(epoll_event_loop&& other) noexcept;

		void poll() override;
		bool has_events() const override;

	private:
		void schedule_event(event_pair event, std::optional<std::chrono::milliseconds> timeout,
							event_type::handle_type& handle) override;

		std::vector<epoll_event> epoll(std::size_t count, std::optional<clock::duration> timeout);
		static generator<std::pair<int, poll_type>> convert(epoll_event event);
		event_list poll(std::size_t count, std::optional<clock::duration> timeout);

		std::optional<std::reference_wrapper<future_type>> remove_event(event_pair event);
		void add_event(event_pair event, std::optional<clock::duration> timeout, future_type& future);

		void remove_before(std::unordered_map<int, timed_future>& map, time_point point);
		void remove_before(time_point point);
		std::optional<time_point> get_timeout(const std::unordered_map<int, timed_future>& map);
		std::optional<time_point> get_timeout();

		inline std::unordered_map<int, timed_future>& get_map(poll_type type) {
			return type == poll_type::read ? _read_events : _write_events;
		}

		static inline uint32_t event_to_epoll(poll_type type) {
			return type == poll_type::read ? EPOLLIN : EPOLLOUT;
		}
	};
#endif

#ifdef COBRA_MACOS
	class kqueue_event_loop : public event_loop {
	public:
		using event_pair = std::pair<int, poll_type>;

		kqueue_event_loop(executor& exec);

		void poll() override;

	private:
		void schedule_event(event_pair event, std::optional<std::chrono::milliseconds> timeout, event_type::handle_type& handle) override;
	};
#endif

#if defined COBRA_LINUX
	using platform_event_loop = epoll_event_loop;
#elif defined COBRA_MACOS
	using platform_event_loop = kqueue_event_loop;
#endif
} // namespace cobra

#endif
