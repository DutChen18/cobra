#ifndef COBRA_TASK_HH
#define COBRA_TASK_HH

#include "cobra/asyncio/result.hh"

#include <coroutine>

namespace cobra {
	template<class T>
	class promise;

	template<class T>
	class awaiter {
		std::coroutine_handle<promise<T>> _task;

	public:
		bool await_ready() {
			return _task.done();
		}

		template<class U>
		void await_suspend(std::coroutine_handle<promise<U>> caller) {
			_task.resume();
			caller.resume();
		}

		T await_resume() {
			return _task.promise().get();
		}
	};

	template<class T>
	class task {
		std::coroutine_handle<promise<T>> _handle;
	
	public:
		using promise_type = promise<T>;

		task(std::coroutine_handle<promise<T>> handle) : _handle(handle) {
		}

		~task() {
			_handle.destroy();
		}

		awaiter<T> operator co_await() {
			return awaiter<T> { _handle };
		}
	};
	
	template<class T>
	class promise : result<T> {
	public:
		task<T> get_return_object() {
			return std::coroutine_handle<promise>::from_promise(*this);
		}

		std::suspend_always initial_suspend() noexcept {
			return {};
		}
		
		std::suspend_always final_suspend() noexcept {
			return {};
		}
	};
}

#endif
