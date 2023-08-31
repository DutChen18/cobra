#ifndef COBRA_COUNTER_HH
#define COBRA_COUNTER_HH

#include <atomic>

namespace cobra {

	template <class T>
	class counter {
		std::atomic<T>& _atomic;
		T _prev_val;

	public:
		counter() = delete;
		counter(const counter& other) = delete;
		counter(std::atomic<T>& atomic) : _atomic(atomic), _prev_val(_atomic.fetch_add(static_cast<T>(1))) {}
		~counter() {
			_atomic.fetch_sub(static_cast<T>(1));
		}

		inline const T prev_val() const {
			return _prev_val;
		}
	};
} // namespace cobra

#endif
