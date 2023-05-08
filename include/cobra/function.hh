#ifndef COBRA_FUNCTION_HH
#define COBRA_FUNCTION_HH

#include "cobra/util.hh"

#include <memory>

namespace cobra {
	template<class Return, class... Args>
	class function {
	private:
		class base {
		public:
			virtual ~base() {}
			virtual Return apply(Args...) = 0;
			// virtual Return apply(Args...) const = 0;
		};

		template<class T>
		class derived : public base {
		private:
			T func;
		public:
			derived(T&& func) : func(std::move(func)) {
			}

			Return apply(Args... args) override {
				return func(std::forward<Args>(args)...);
			}
		
			/*
			Return apply(Args... args) const override {
				return func(std::forward<Args>(args)...);
			}
			*/
		};

		std::unique_ptr<base> inner;
	public:
		function() = default;
		function(const function&) = delete;

		template<class T>
		function(T&& func) : inner(new derived<T>(std::move(func))) {
		}

		function(function&& other) : inner(std::move(other.inner)) {
		}

		function& operator=(function other) {
			std::swap(inner, other.inner);
			return *this;
		}

		Return operator()(Args... args) {
			return inner->apply(std::forward<Args>(args)...);
		}

		Return operator()(Args... args) const {
			return inner->apply(std::forward<Args>(args)...);
		}

		bool empty() const {
			return inner.get() == nullptr;
		}
	};

	template<class F, class... T>
	class captured {
	private:
		F func;
		std::tuple<T...> args;

		template<class... U, std::size_t... I>
		auto operator()(index_sequence<I...>, U&&... args) -> decltype(func(std::declval<T&>()..., std::forward<U>(args)...)) {
			return func(std::get<I>(this->args)..., std::forward<U>(args)...);
		}

		template<class... U, std::size_t... I>
		auto operator()(index_sequence<I...>, U&&... args) const -> decltype(func(std::declval<const T&>()..., std::forward<U>(args)...)) {
			return func(std::get<I>(this->args)..., std::forward<U>(args)...);
		}
	public:
		captured(F&& func, T&&... args) : func(std::move(func)), args(std::make_tuple(std::forward<T>(args)...)) {
		}

		captured(const captured&) = delete;

		captured(captured&& other) : func(std::move(other.func)), args(std::move(other.args)) {
		}

		captured& operator=(captured other) {
			std::swap(func, other.func);
			std::swap(args, other.args);
			return *this;
		}

		template<class... U>
		auto operator()(U&&... args) -> decltype(func(std::declval<T&>()..., std::forward<U>(args)...)) {
			return (*this)(typename make_index_sequence<sizeof...(T)>::type(), std::forward<U>(args)...);
		}
		
		template<class... U>
		auto operator()(U&&... args) const -> decltype(func(std::declval<const T&>()..., std::forward<U>(args)...)) {
			return (*this)(typename make_index_sequence<sizeof...(T)>::type(), std::forward<U>(args)...);
		}
	};
	
	template<class F, class... T>
	captured<F, T...> capture(F&& func, T&&... args) {
		return captured<F, T...>(std::forward<F>(func), std::forward<T>(args)...);
	}
}

#endif
