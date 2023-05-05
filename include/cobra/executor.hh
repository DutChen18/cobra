#ifndef COBRA_EXECUTOR_HH
#define COBRA_EXECUTOR_HH

#include <functional>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>

namespace cobra {
	class executor {
	public:
		virtual ~executor();
		virtual void exec(std::function<void()> func) = 0;
	};

	class sequential_executor : public executor {
	public:
		void exec(std::function<void()> func) override;
	};

	class thread_pool_executor : public executor {
	private:
		std::queue<std::function<void()>> funcs;
		std::vector<std::thread> threads;
		std::mutex mutex;
		std::condition_variable condition_variable;
		bool stopped;
	public:
		thread_pool_executor(std::size_t size);
		~thread_pool_executor();

		void exec(std::function<void()> func) override;
	};
}

#endif
