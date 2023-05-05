#ifndef COBRA_EVENT_LOOP_HH
#define COBRA_EVENT_LOOP_HH

#include <mutex>
#include <vector>
#include <functional>
#include <unordered_map>

#include "cobra/optional.hh"

namespace cobra {

	class event_loop {
	protected:
		enum class listen_type {
			write,
			read,
		};

	public:
		using callback_type = std::function<void()>;

		event_loop() = default;
		event_loop(const event_loop& other) = delete;
		event_loop(event_loop&& other) noexcept;
		virtual ~event_loop();

		event_loop &operator=(const event_loop& other) = delete;

		virtual void on_read_ready(int fd, callback_type callback);
		virtual void on_write_ready(int fd, callback_type callback);

		virtual void run() = 0;
	protected:
		virtual void on_ready(int fd, listen_type, callback_type callback) = 0;
	};

	class epoll_event_loop : public event_loop {
		using callback_map = std::unordered_map<int, callback_type>;
		using event_type = std::pair<int, listen_type>;

		std::mutex mtx;
		callback_map read_callbacks;
		callback_map write_callbacks;
		int epoll_fd;

	public:
		epoll_event_loop();
		//epoll_event_loop(epoll_event_loop&& other) noexcept;
		~epoll_event_loop();

		void run() override;

	protected:
		std::vector<event_type> poll(std::size_t count);
		bool done() const;
		int get_events(listen_type type) const;
		listen_type get_type(int event) const;

		callback_map& get_map(listen_type type);
		optional<callback_type> get_callback(event_type event);
		bool remove_callback(event_type event);

		void on_ready(int fd, listen_type, callback_type callback) override;
	};
}

#endif
