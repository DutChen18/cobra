#ifndef COBRA_EVENT_LOOP_HH
#define COBRA_EVENT_LOOP_HH
#include "cobra/asyncio/future.hh"
#include "cobra/asyncio/task.hh"
#include "cobra/file.hh"
#include "cobra/asyncio/generator.hh"
#include <functional>
#include <unordered_map>
#include <optional>
#include <chrono>
#include <map>
#include <mutex>
#include <cstdint>
#include <memory>

extern "C" {
#include <sys/epoll.h>
}

namespace cobra {

	enum class event_type {
		read,
		write,
	};

	event_type operator!(event_type type);

	class event_loop {
	public:
		virtual ~event_loop();

		virtual task<void> wait_ready(event_type type, const file& fd, std::optional<std::chrono::milliseconds> timeout = std::nullopt) = 0;
		virtual void poll() = 0;

		virtual task<void> wait_read(const file& fd, std::optional<std::chrono::milliseconds> timeout = std::nullopt);
		virtual task<void> wait_write(const file& fd, std::optional<std::chrono::milliseconds> timeout = std::nullopt);
	};

	class epoll_event_loop : public event_loop {
	public:
		using clock = std::chrono::steady_clock;
		using future_type = future<void>;
		using time_point = clock::time_point;
		using event_pair = std::pair<int, event_type>;

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

		task<void> wait_ready(event_type type, const file& fd, std::optional<std::chrono::milliseconds> timeout) override;
		void poll() override;

	private:
		std::vector<epoll_event> epoll(std::size_t count, std::optional<clock::duration> timeout);
		static generator<std::pair<int, event_type>> convert(epoll_event event);
		event_list poll(std::size_t count, std::optional<clock::duration> timeout);

		std::optional<std::reference_wrapper<future_type>> remove_event(event_pair event);
		void add_event(event_pair event, std::optional<clock::duration> timeout, future_type& future);

		inline std::unordered_map<int, timed_future> &get_map(event_type event) {
			switch (event) {
				case event_type::read:
					return _read_events;
				case event_type::write:
					return _write_events;
			}
		}

		void remove_before(std::unordered_map<int, timed_future> &map, time_point point);
		void remove_before(time_point point);
		std::optional<time_point> get_timeout(const std::unordered_map<int, timed_future>& map);
		std::optional<time_point> get_timeout();

		static inline uint32_t event_to_epoll(event_type type) {
			switch (type) {
				case event_type::read:
					return EPOLLIN;
				case event_type::write:
					return EPOLLOUT;
			}
		}
	};
}

#endif
