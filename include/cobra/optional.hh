#ifndef COBRA_OPTIONAL_HH
#define COBRA_OPTIONAL_HH

#include "cobra/util.hh"

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
					std::swap(**this, *other);
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
	
	template<class T, class U>
	class result {
	private:
		optional<T> ok_value;
		optional<U> err_value;
	public:
		result(optional<T>&& ok_value, optional<U>&& err_value) {
			this->ok_value = std::move(ok_value);
			this->err_value = std::move(err_value);
		}

		result(const result& other) {
			ok_value = other.ok_value;
			err_value = other.err_value;
		}

		result(result&& other) {
			ok_value = std::move(other.ok_value);
			err_value = std::move(other.err_value);
		}

		result& operator=(result other) {
			std::swap(ok_value, other.ok_value);
			std::swap(err_value, other.err_value);
			return *this;
		}

		explicit operator bool() const noexcept {
			return bool(ok_value);
		}

		const T* operator->() const noexcept {
			return &*ok_value;
		}

		T* operator->() noexcept {
			return &*ok_value;
		}

		const T& operator*() const noexcept {
			return *ok_value;
		}

		T& operator*() noexcept {
			return *ok_value;
		}

		const optional<U>& err() const noexcept {
			return err_value;
		}

		optional<U>& err() noexcept {
			return err_value;
		}
	};

	template<class T, class... U>
	optional<T> some(U&&... args) {
		return optional<T>(T(std::forward<U>(args)...));
	}

	template<class T>
	optional<T> none() {
		return optional<T>();
	}

	template<class T, class U, class... V>
	result<T, U> ok(V&&... args) {
		return result<T, U>(some<T>(std::forward<V>(args)...), none<U>());
	}
	
	template<class T, class U, class... V>
	result<T, U> err(V&&... args) {
		return result<T, U>(none<T>(), some<U>(std::forward<V>(args)...));
	}
}

#endif
