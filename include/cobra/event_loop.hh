#ifndef COBRA_EVENT_LOOP_HH
#define COBRA_EVENT_LOOP_HH

#include <vector>
#include <unordered_map>

#include "cobra/runner.hh"
#include "cobra/optional.hh"
#include "cobra/function.hh"

namespace cobra {
	enum class listen_type {
		write,
		read,
	};

	class event_loop {
	private:
		const runner& rnr;

	public:
		using callback_type = function<void>;

		event_loop() = delete;
		event_loop(const event_loop& other) = delete;
		event_loop(const runner& runner);
		event_loop(event_loop&& other) noexcept;
		virtual ~event_loop();

		event_loop &operator=(const event_loop& other) = delete;
		
		virtual void on_ready(int fd, listen_type, callback_type&& callback) = 0;

		virtual void poll() = 0;
		virtual bool done() const = 0;

		void lock() const;
		void unlock() const;
	protected:
		inline const runner& get_runner() const { return rnr; }
	};

	class epoll_event_loop : public event_loop {
		using callback_map = std::unordered_map<int, callback_type>;
		using event_type = std::pair<int, listen_type>;

		callback_map read_callbacks;
		callback_map write_callbacks;
		int epoll_fd;

	public:
		epoll_event_loop(const runner& rnr);
		//epoll_event_loop(epoll_event_loop&& other) noexcept;
		~epoll_event_loop();
		
		void on_ready(int fd, listen_type, callback_type&& callback) override;

		void poll() override;
		bool done() const override;

	protected:
		std::vector<event_type> poll(std::size_t count);
		int get_events(listen_type type) const;
		listen_type get_type(int event) const;

		callback_map& get_map(listen_type type);
		optional<callback_type> remove_callback(event_type event);
	};
}

#endif