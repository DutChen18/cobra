#ifndef COBRA_FUNCTION_HH
#define COBRA_FUNCTION_HH

#include "cobra/util.hh"

#include <memory>

namespace cobra {
	template<class Return, class... Args>
	class base_function {
	public:
		virtual ~base_function() {}
		virtual Return apply(Args&&... args) = 0;
	};

	template<class T, class Return, class... Args>
	class derived_function : public base_function<Return, Args...> {
	private:
		T func;
	public:
		derived_function(T&& func) : func(std::forward<T>(func)) {
		}

		Return apply(Args&&... args) override {
			return func(std::forward<Args>(args)...);
		}
	};

	template<class Return, class... Args>
	class shared_function;

	template<class Return, class... Args>
	class function {
	public:
		using shared = shared_function<Return, Args...>;
	private:
		std::unique_ptr<base_function<Return, Args...>> inner;
	public:
		function() = default;
		function(const function&) = delete;

		template<class T>
		function(T&& func) : inner(make_unique<derived_function<T, Return, Args...>>(std::forward<T>(func))) {
		}

		function(function&& other) : inner(std::move(other.inner)) {
		}

		function& operator=(function other) {
			std::swap(inner, other.inner);
			return *this;
		}

		Return operator()(Args... args) const {
			// TODO: handle null inner
			return inner->apply(std::forward<Args>(args)...);
		}

		bool empty() const {
			return !inner;
		}

		std::unique_ptr<base_function<Return, Args...>> leak_inner() {
			return std::move(inner);
		}
	};

	template<class Return, class... Args>
	class shared_function {
	private:
		std::shared_ptr<base_function<Return, Args...>> inner;
	public:
		shared_function() = default;

		shared_function(const shared_function& other) : inner(other.inner) {
		}

		shared_function(shared_function&& other) : inner(std::move(other.inner)) {
		}

		shared_function(function<Return, Args...>&& other) : inner(other.leak_inner()) {
		}

		shared_function& operator=(shared_function other) {
			std::swap(inner, other.inner);
			return *this;
		}

		Return operator()(Args... args) const {
			// TODO: handle null inner
			return inner->apply(std::forward<Args>(args)...);
		}

		bool empty() const {
			return !inner;
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
		captured(F&& func, T&&... args) : func(std::move(func)), args { std::forward<T>(args)... } {
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
