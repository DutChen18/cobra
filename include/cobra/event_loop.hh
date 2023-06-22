#ifndef COBRA_EVENT_LOOP_HH
#define COBRA_EVENT_LOOP_HH

#include "cobra/asyncio/future.hh"
#include "cobra/file.hh"
#include <unordered_map>
#include <optional>
#include <chrono>

namespace cobra {

	enum class event_type {
		write,
		read,
	};

	class event_loop {
	public:
		virtual ~event_loop();

		virtual future<void> wait_ready(event_type type, const file& fd, std::optional<std::chrono::milliseconds> timeout) = 0;
		virtual void poll() = 0;
	};

	class epoll_event_loop : public event_loop {
		file _epoll_fd;

		std::unordered_map<int, future<void>> _write_events;
		std::unordered_map<int, future<void>> _read_events;

	public:
		epoll_event_loop();
		epoll_event_loop(const epoll_event_loop& other) = delete;
		epoll_event_loop(epoll_event_loop&& other) noexcept;

		future<void> wait_ready(event_type type, const file& fd, std::optional<std::chrono::milliseconds> timeout) override;
		void poll() override;

	private:
		void epoll();
	};
}

#endif
