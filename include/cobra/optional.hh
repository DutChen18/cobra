#ifndef COBRA_OPTIONAL_HH
#define COBRA_OPTIONAL_HH

#include <utility>
#include <tuple>

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
			new(storage) T(std::move(value));
		}

		optional(const optional& other) {
			has_value = other.has_value;

			if (has_value) {
				new(storage) T(*other);
			}
		}

		optional(optional&& other) {
			has_value = other.has_value;

			if (has_value) {
				new(storage) T(std::move(*other));
			}
		}

		~optional() {
			if (has_value) {
				(*this)->~T();
			}
		}

		optional& operator=(optional other) {
			if (other.has_value) {
				if (has_value) {
					std::swap(**this, **other);
				} else {
					new(storage) T(std::move(*other));
				}
			} else {
				if (has_value) {
					(*this)->~T();
				}
			}

			std::swap(has_value, other.has_value);

			return *this;
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

	typedef std::tuple<> unit;
	typedef optional<unit> optional_unit;

	template<class T>
	optional<T> some(T value = T()) {
		return optional<T>(std::move(value));
	}

	template<class T>
	optional<T> none() {
		return optional<T>();
	}
}

#endif
