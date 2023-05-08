#ifndef COBRA_FUTURE_HH
#define COBRA_FUTURE_HH

#include "cobra/executor.hh"
#include "cobra/event_loop.hh"
#include "cobra/util.hh"
#include "cobra/function.hh"

#include <functional>
#include <atomic>

namespace cobra {
	template<class... T>
	class context {
	private:
		executor* exec;
		event_loop *loop;
		function<void, T...> func;
	public:
		context(executor* exec, event_loop* loop, function<void, T...>&& func = {}) {
			this->exec = exec;
			this->loop = loop;
			this->func = std::move(func);
		}

		context(const context&) = delete;

		context(context&& other) {
			exec = other.exec;
			loop = other.loop;
			func = std::move(other.func);
		}

		context& operator=(context other) {
			std::swap(exec, other.exec);
			std::swap(loop, other.loop);
			std::swap(func, other.func);
			return *this;
		}

		void run(function<void>&& func) const {
			if (exec != nullptr && !func.empty()) {
				exec->exec(std::move(func));
			}
		}

		void on_ready(int fd, listen_type type, function<void>&& func) const {
			if (exec != nullptr && !func.empty()) {
				executor* exec = this->exec;

				loop->on_ready(fd, type, capture([exec](function<void>& func) {
					exec->exec(std::move(func));
				}, std::move(func)));
			}
		}

		void resolve(T... args) {
			if (exec != nullptr && !func.empty()) {
				run(capture([args...](function<void, T...>& func) {
					func(args...);
				}, std::move(func)));
			}
		}

		template<class... U>
		context<U...> detach(typename type_identity<function<void, U...>>::type&& func) const {
			return context<U...>(exec, loop, std::move(func));
		}

		context<> detach() const {
			return context<>(exec, loop);
		}
	};

	template<class... T>
	class future {
		function<void, context<T...>&&> func;
	public:
		using tuple_type = std::tuple<T...>;

		future(const future&) = delete;

		future(future&& other) : func(std::move(other.func)) {
		}

		future(T... args) {
			this->func = [args...](context<T...>&& ctx) {
				ctx.resolve(args...);
			};
		}

		future(function<void, context<T...>&&>&& func) : func(std::move(func)) {
		}

		future& operator=(future other) {
			std::swap(func, other.func);
			return *this;
		}

		void start(context<T...>&& ctx)&& {
			func(std::move(ctx));
		}

		void run(context<T...>&& ctx)&& {
			ctx.run(capture([](future<T...>& self, context<T...>& ctx) {
				std::move(self).start(std::move(ctx));
			}, std::move(*this), std::move(ctx)));
		}

		template<class... U>
		future<U...> then(typename type_identity<function<future<U...>, T...>>::type&& func)&& {
			using func_type = typename type_identity<function<future<U...>, T...>>::type;

			return future<U...>(capture([](future<T...>& self, func_type& func, context<U...>&& ctx) {
				std::move(self).start(ctx.template detach<T...>(capture([](func_type& func, context<U...>& ctx, T... args) {
					func(args...).start(std::move(ctx));
				}, std::move(func), std::move(ctx))));
			}, std::move(*this), std::move(func)));
		}

		future<tuple_type> tie()&& {
			return then<tuple_type>([](T&&... args) {
				return future<tuple_type>(std::make_tuple(std::forward<T>(args)...));
			});
		}
	};

	template<class... Future, std::size_t... I>
	future<typename Future::tuple_type...> all_impl(index_sequence<I...>, Future... futs) {
		return future<typename Future::tuple_type...>(
			[futs...](context<typename Future::tuple_type...> ctx) {
				std::shared_ptr<std::tuple<typename Future::tuple_type...>> result = std::make_shared<std::tuple<typename Future::tuple_type...>>();
				std::shared_ptr<std::atomic_size_t> progress = std::make_shared<std::atomic_size_t>(0);

				std::make_tuple((futs.tie().run(
					ctx.template detach<typename Future::tuple_type>(
						[ctx, result, progress](typename Future::tuple_type arg) {
							std::get<I>(*result) = arg;

							if (++*progress == sizeof...(Future)) {
								ctx.resolve(std::get<I>(*result)...);
							}
						}
					)
				), 0)...);
			}
		);
	}

	template<class... Future>
	future<typename Future::tuple_type...> all(Future... futs) {
		return all_impl(typename make_index_sequence<sizeof...(Future)>::type(), futs...);
	}

	template<class... Future, std::size_t... I>
	typename rename_tuple<decltype(std::tuple_cat(std::declval<typename Future::tuple_type>()...))>::template type<future> all_flat_impl(index_sequence<I...>, Future... futs) {
		using tuple_type = decltype(std::tuple_cat(std::declval<typename Future::tuple_type>()...));

		return all(futs...).template then<typename std::tuple_element<I, tuple_type>::type...>(
			[](typename Future::tuple_type... args) {
				tuple_type result = std::tuple_cat(args...);

				return future<typename std::tuple_element<I, tuple_type>::type...>(std::get<I>(result)...);

				(void) result;
			}
		);
	}

	template<class... Future>
	typename rename_tuple<decltype(std::tuple_cat(std::declval<typename Future::tuple_type>()...))>::template type<future> all_flat(Future... futs) {
		using tuple_type = decltype(std::tuple_cat(std::declval<typename Future::tuple_type>()...));

		return all_flat_impl(typename make_index_sequence<std::tuple_size<tuple_type>::value>::type(), futs...);
	}

	inline future<> async_while(function<future<bool>>&& func) {
		return func().then(capture([](function<future<bool>>& func, bool cond) {
			if (cond) {
				return async_while(std::move(func));
			} else {
				return future<>();
			}
		}, std::move(func)));
	}
}

#endif
