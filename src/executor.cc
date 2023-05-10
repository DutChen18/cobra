#include "cobra/executor.hh"
#include "cobra/context.hh"

#include <exception>
#include <iostream>

namespace cobra {
	executor::executor(context* ctx) {
		this->ctx = ctx;
	}

	executor::~executor() {
	}

	sequential_executor::sequential_executor(context* ctx) : executor(ctx) {
	}

	void sequential_executor::exec(function<void>&& func) {
		func(); // TODO: handle uncaught exceptions
	}

	bool sequential_executor::done() const {
		return true;
	}

	thread_pool_executor::thread_pool_executor(context* ctx, std::size_t size) : executor(ctx) {
		stopped = false;
		count = 0;

		for (std::size_t i = 0; i < size; i++) {
			threads.push_back(std::thread([this]() {
				std::unique_lock<std::mutex> guard(this->ctx->get_mutex());

				while (!stopped) {
					if (!funcs.empty()) {
						function<void> func(std::move(funcs.front()));

						funcs.pop();
						guard.unlock();
						func(); // TODO: handle uncaught exceptions
						guard.lock();

						count -= 1;
						this->ctx->get_condition_variable().notify_all();

						continue;
					}
					
					condition_variable.wait(guard);
				}
			}));
		}
	}

	thread_pool_executor::~thread_pool_executor() {
		{
			std::unique_lock<std::mutex> guard(this->ctx->get_mutex());

			stopped = true;
			condition_variable.notify_all();
		}

		for (std::thread& thread : threads) {
			thread.join();
		}
	}
	
	void thread_pool_executor::exec(function<void>&& func) {
		std::unique_lock<std::mutex> guard(this->ctx->get_mutex());

		funcs.push(std::move(func));
		count += 1;
		condition_variable.notify_one();
	}

	bool thread_pool_executor::done() const {
		return count == 0;
	}
}
