#ifndef COBRA_ASYNCIO_COROUTINE_HH
#define COBRA_ASYNCIO_COROUTINE_HH

#include <coroutine>
#include <utility>

namespace cobra {
	template <class Promise>
	class coroutine {
		std::coroutine_handle<Promise> _handle;

	public:
		using promise_type = Promise;

		coroutine() noexcept = default;

		coroutine(const coroutine& other) noexcept = delete;

		coroutine(coroutine&& other) noexcept {
			_handle = std::exchange(_handle, other._handle);
		}

		coroutine(Promise& promise) noexcept {
			_handle = std::coroutine_handle<Promise>::from_promise(promise);
		}

		~coroutine() noexcept {
			if (_handle) {
				_handle.destroy();
			}
		}

		coroutine& operator=(coroutine other) noexcept {
			std::swap(_handle, other._handle);
			return *this;
		}

		std::coroutine_handle<Promise> handle() const noexcept {
			return _handle;
		}
	};
} // namespace cobra

#endif
