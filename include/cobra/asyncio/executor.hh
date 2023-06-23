#ifndef COBRA_ASYNCIO_EXECUTOR_HH
#define COBRA_ASYNCIO_EXECUTOR_HH

#include "cobra/asyncio/task.hh"

#include <functional>

namespace cobra {
	class executor {
	public:
		virtual ~executor();

	protected:
		virtual void _start(std::function<void()> func) = 0;
	};

	class synchronous_executor : public executor {
	protected:
		virtual void _start(std::function<void()> func) override {
			func();
		}
	};
}

#endif
