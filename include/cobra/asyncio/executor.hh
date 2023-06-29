#ifndef COBRA_ASYNCIO_EXECUTOR_HH
#define COBRA_ASYNCIO_EXECUTOR_HH

#include "cobra/asyncio/event.hh"
#include "cobra/asyncio/async_task.hh"

#include <thread>
#include <vector>
#include <condition_variable>
#include <queue>

namespace cobra {
	class executor {
	protected:
		struct executor_event {
			executor* _exec;

			void operator()(event_handle<void>& handle);
		};

	public:
		using event_type = event<void, executor_event>;

		virtual ~executor();

		event_type schedule();

		template<class Awaitable>
		auto schedule_task(Awaitable awaitable) -> async_task<decltype(awaitable.await_resume())> {
			co_await schedule();
			co_return co_await awaitable;
		}

	private:
		virtual void schedule_event(event_type::handle_type& handle) = 0;
	};

	class sequential_executor : public executor {
	private:
		void schedule_event(event_type::handle_type& handle) override;
	};

	class thread_pool_executor : public executor {
		std::vector<std::jthread> _threads;
		std::queue<event_type::handle_type*> _queue;
		std::mutex _mutex;
		std::condition_variable _condition_variable;

	public:
		thread_pool_executor();
		thread_pool_executor(std::size_t count);
		~thread_pool_executor();

	private:
		void schedule_event(event_type::handle_type& handle) override;
		void create_threads(std::size_t count);
	};
}

#endif
