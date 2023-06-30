#ifndef COBRA_ASYNCIO_ASYNC_TASK_HH
#define COBRA_ASYNCIO_ASYNC_TASK_HH

#include "cobra/asyncio/coroutine.hh"
#include "cobra/asyncio/promise.hh"

#include <atomic>

namespace cobra {
	template <class T>
	class async_task_promise;

	template <class T>
	class async_task {
		std::coroutine_handle<async_task_promise<T>> _handle;

	public:
		using promise_type = async_task_promise<T>;

		async_task() noexcept = default;

		async_task(const async_task& other) noexcept = delete;

		async_task(async_task&& other) noexcept {
			_handle = std::exchange(other._handle, nullptr);
		}

		async_task(async_task_promise<T>& promise) noexcept {
			_handle = std::coroutine_handle<async_task_promise<T>>::from_promise(promise);
		}

		~async_task() {
			if (_handle.promise().destroy_flag().test_and_set()) {
				_handle.destroy();
			}
		}

		async_task& operator=(async_task other) noexcept {
			std::swap(_handle, other._handle);
			return *this;
		}

		bool await_ready() const noexcept {
			return false;
		}

		void await_suspend(std::coroutine_handle<> handle) const noexcept {
			_handle.promise().set_next(handle);

			if (_handle.promise().done_flag().test_and_set()) {
				handle.resume();
			}
		}

		T await_resume() const {
			return _handle.promise().result().get_value_move();
		}
	};

	class async_task_final_suspend {
	public:
		bool await_ready() const noexcept {
			return false;
		}

		template <class T>
		void await_suspend(std::coroutine_handle<async_task_promise<T>> handle) const noexcept {
			if (handle.promise().done_flag().test_and_set()) {
				handle.promise().next().resume();
			}

			if (handle.promise().destroy_flag().test_and_set()) {
				handle.destroy();
			}
		}

		void await_resume() const noexcept {
			return;
		}
	};

	template <class T>
	class async_task_promise : public promise<T> {
		std::atomic_flag _done_flag;
		std::atomic_flag _destroy_flag;

	public:
		async_task<T> get_return_object() noexcept {
			return {*this};
		}

		auto initial_suspend() const noexcept {
			return std::suspend_never();
		}

		auto final_suspend() const noexcept {
			return async_task_final_suspend();
		}

		std::atomic_flag& done_flag() noexcept {
			return _done_flag;
		}

		std::atomic_flag& destroy_flag() noexcept {
			return _destroy_flag;
		}
	};
} // namespace cobra

#endif
