#ifndef COBRA_MEMORY_HH
#define COBRA_MEMORY_HH

#include "cobra/thread.hh"

#include <cstddef>

namespace cobra {
	template<class T>
	class moved {
	private:
		T& inner;
	public:
		moved(T& ref) : inner(ref) {
		}
		
		moved(const moved& other) : inner(other.inner) {
		}

		const T& operator*() const {
			return inner;
		}

		T& operator*() {
			return inner;
		}

		const T* operator->() const {
			return &inner;
		}

		T* operator->() {
			return &inner;
		}
	};

	template<class T>
	class shared {
	private:
		struct ctrl_block {
			atomic<std::size_t> count;
			T inner;

			ctrl_block() {
				count = 1;
			}

			ctrl_block(moved<T> value) : inner(value) {
				count = 1;
			}
		};

		ctrl_block* ctrl;
	public:
		shared() {
			ctrl = new ctrl_block();
		}

		shared(moved<T> value) {
			ctrl = new ctrl_block(value);
		}

		shared(const shared& other) {
			ctrl = other.ctrl;
			++ctrl->count;
		}

		~shared() {
			if (--ctrl->count == 0) {
				delete ctrl;
			}
		}

		const T& operator*() const {
			return ctrl->inner;
		}

		T& operator*() {
			return ctrl->inner;
		}

		const T* operator->() const {
			return &ctrl->inner;
		}

		T* operator->() {
			return &ctrl->inner;
		}
	};

	template<class T>
	class unique_ptr {
	private:
		T* inner;
	public:
		unique_ptr() {
			inner = NULL;
		}

		template<class U>
		unique_ptr(moved<U> value) {
			inner = new U(value);
		}

		unique_ptr(moved<unique_ptr> other) {
			inner = other->inner;
			other->inner = NULL;
		}
		
		~unique_ptr() {
			delete inner;
		}

		const T& operator*() const {
			return *inner;
		}

		T& operator*() {
			return *inner;
		}

		const T* operator->() const {
			return inner;
		}

		T* operator->() {
			return inner;
		}
	};

	template<class T>
	class shared_ptr {
	private:
		shared<unique_ptr<T> > inner;
	public:
		shared_ptr() {
		}

		template<class U>
		shared_ptr(moved<U> value) {
			unique_ptr<T> tmp = value;
			inner = moved<unique_ptr<T> >(tmp);
		}

		shared_ptr(const shared_ptr& other) {
			inner = other.inner;
		}

		const T& operator*() const {
			return **inner;
		}

		T& operator*() {
			return **inner;
		}

		const T* operator->() const {
			return &**inner;
		}

		T* operator->() {
			return &**inner;
		}
	};
}

#endif
