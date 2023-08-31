#ifndef COBRA_ASYNCIO_MUTEX_HH
#define COBRA_ASYNCIO_MUTEX_HH

#include "cobra/asyncio/executor.hh"
#include "cobra/asyncio/task.hh"

namespace cobra {
	class async_mutex {
		executor* _exec;
		std::queue<event_handle<void>*> _queue;
		std::mutex _mutex;
		bool _locked = false;

		struct lock_event {
			async_mutex* _mutex;

			void operator()(event_handle<void>& handle);
		};

	public:
		async_mutex(executor* exec = &global_executor);

		event<void, lock_event> lock();
		bool try_lock();
		void unlock();
	};

	class async_lock {
		async_mutex* _mutex;

		async_lock(async_mutex& mutex);

	public:
		async_lock(const async_lock& other) = delete;
		async_lock(async_lock&& other);
		~async_lock();

		async_lock& operator=(async_lock other);

		async_mutex* mutex() const;

		static task<async_lock> lock(async_mutex& mutex);
	};

	class async_condition_variable {
		executor* _exec;
		std::queue<std::pair<event_handle<void>*, async_lock*>> _queue;
		std::mutex _mutex;

		struct wait_event {
			async_condition_variable* _condition_variable;
			async_lock* _lock;

			void operator()(event_handle<void>& handle);
		};

	public:
		async_condition_variable(executor* exec = &global_executor);

		void notify_one();
		void notify_all();

		event<void, wait_event> wait(async_lock& lock);
	};
} // namespace cobra

#endif
