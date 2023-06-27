#ifndef COBRA_ASYNCIO_EXECUTOR_HH
#define COBRA_ASYNCIO_EXECUTOR_HH

#include "cobra/asyncio/event.hh"
#include "cobra/asyncio/async_task.hh"

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
		auto schedule_task(Awaitable&& awaitable) -> async_task<decltype(awaitable.await_resume())> {
			co_await schedule();
			co_return co_await awaitable;
		}

	private:
		virtual void schedule_event(event_type::handle_type& handle) = 0;
	};

	class sequential_executor : public executor {
	public:
		void schedule_event(event_type::handle_type& handle);
	};
}

#endif
