#ifndef COBRA_EVENT_LOOP_HH
#define COBRA_EVENT_LOOP_HH
#include "cobra/asyncio/event.hh"
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

extern "C" {
#include <sys/epoll.h>
}

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
			event_loop* _loop;
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

		virtual void poll() = 0;

	private:
		virtual void schedule_event(event_pair event, std::optional<std::chrono::milliseconds> timeout,
									event_type::handle_type& handle) = 0;
	};

	class epoll_event_loop : public event_loop {
	public:
		using clock = std::chrono::steady_clock;
		using future_type = event_handle<void>;
		using time_point = clock::time_point;
		using event_pair = std::pair<int, poll_type>;

	private:
		file _epoll_fd;
		std::mutex _mutex;

		struct timed_future {
			std::reference_wrapper<future_type> future;
			std::optional<time_point> timeout;
		};

		std::unordered_map<int, timed_future> _write_events;
		std::unordered_map<int, timed_future> _read_events;

		using event_list = std::vector<event_pair>;

	public:
		epoll_event_loop();
		epoll_event_loop(const epoll_event_loop& other) = delete;
		epoll_event_loop(epoll_event_loop&& other) noexcept;

		void poll() override;

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

		inline std::unordered_map<int, timed_future>& get_map(poll_type event) {
			switch (event) {
			case poll_type::read:
				return _read_events;
			case poll_type::write:
				return _write_events;
			}
		}

		static inline uint32_t event_to_epoll(poll_type type) {
			switch (type) {
			case poll_type::read:
				return EPOLLIN;
			case poll_type::write:
				return EPOLLOUT;
			}
		}
	};
} // namespace cobra

#endif
