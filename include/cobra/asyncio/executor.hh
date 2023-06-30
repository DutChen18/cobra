#ifndef COBRA_ASYNCIO_EXECUTOR_HH
#define COBRA_ASYNCIO_EXECUTOR_HH

#include "cobra/asyncio/async_task.hh"
#include "cobra/asyncio/event.hh"

#include <condition_variable>
#include <functional>
#include <queue>
#include <thread>
#include <vector>

namespace cobra {
	class executor {
	protected:
		struct executor_event {
			std::reference_wrapper<executor> _exec;

			void operator()(event_handle<void>& handle);
		};

	public:
		using event_type = event<void, executor_event>;

		virtual ~executor();

		event_type schedule();

		template <class Awaitable>
		auto schedule(Awaitable awaitable) -> async_task<decltype(awaitable.await_resume())> {
			co_await schedule();
			co_return co_await awaitable;
		}

		virtual void schedule(std::function<void()> func) = 0;
	};

	class sequential_executor : public executor {
	public:
		virtual void schedule(std::function<void()> func) override;
	};

	class thread_pool_executor : public executor {
		std::vector<std::jthread> _threads;
		std::queue<std::function<void()>> _queue;
		std::mutex _mutex;
		std::condition_variable _condition_variable;

		void create_threads(std::size_t count);

	public:
		thread_pool_executor();
		thread_pool_executor(std::size_t count);
		~thread_pool_executor();

		virtual void schedule(std::function<void()> func) override;
	};
} // namespace cobra

#endif
