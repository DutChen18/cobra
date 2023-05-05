#ifndef COBRA_OPTIONAL_HH
#define COBRA_OPTIONAL_HH

namespace cobra {
	template<class T>
	class optional {
	private:
		bool has_value;
		alignas(T) unsigned char storage[sizeof(T)];
	public:
		optional() {
			has_value = false;
		}

		optional(T& value) {
			has_value = true;
			new(storage) T(value);
		}

		optional(T&& value) {
			has_value = true;
			new(storage) T(value);
		}

		~optional() {
			if (has_value) {
				(*this)->~T();
			}
		}

		explicit operator bool() const noexcept {
			return has_value;
		}

		const T* operator->() const noexcept {
			return reinterpret_cast<const T*>(storage);
		}

		T* operator->() noexcept {
			return reinterpret_cast<T*>(storage);
		}

		const T& operator*() const noexcept {
			return *reinterpret_cast<const T*>(storage);
		}

		T& operator*() noexcept {
			return *reinterpret_cast<T*>(storage);
		}
	};
}

#endif
