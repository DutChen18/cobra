#ifndef COBRA_ASYNCIO_COROUTINE_HH
#define COBRA_ASYNCIO_COROUTINE_HH

#include <coroutine>
#include <utility>


#include <iostream>

namespace cobra {
	template<class Promise>
	class coroutine {
	protected:
		std::coroutine_handle<Promise> _handle;

	public:
		coroutine(std::coroutine_handle<Promise> handle) : _handle(handle) {
			std::cout << "construct coro " << _handle.address() << std::endl;
		}

		coroutine(const coroutine& other) = delete;

		coroutine(coroutine&& other) {
			_handle = std::exchange(other._handle, nullptr);
			std::cout << "move coro " << _handle.address() << std::endl;
		}

		~coroutine() {
			std::cout << "destroy coro " << _handle.address() << std::endl;
			// _handle.destroy(); // TODO: wut?
		}

		coroutine& operator=(coroutine other) {
			std::swap(_handle, other._handle);
			std::cout << "assign coro " << _handle.address() << std::endl;
			return *this;
		}
	};

	class coroutine_promise {
	public:
		std::suspend_never initial_suspend() const noexcept {
			return {};
		}

		std::suspend_always final_suspend() const noexcept {
			return {};
		}
	};
}

#endif
