#ifndef COBRA_FUTURE_HH
#define COBRA_FUTURE_HH

#include "cobra/executor.hh"
#include "cobra/event_loop.hh"
#include "cobra/util.hh"
#include "cobra/function.hh"

#include <atomic>
#include <functional>

// TODO: exception propagation

namespace cobra {
	template<class... T>
	class context {
	private:
		executor* exec;
		event_loop *loop;
		function<void, T...> func;

		static void execute(executor* exec, function<void>&& func) {
			if (exec == nullptr) {
				func();
			} else {
				exec->exec(std::move(func));
			}
		}

		template<std::size_t... I>
		void resolve(index_sequence<I...>, std::tuple<T...> args) const {
			if (!func.empty()) {
				func(std::get<I>(args)...);
			}
		}
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
			if (!func.empty()) {
				execute(exec, std::move(func));
			}
		}

		void on_ready(int fd, listen_type type, function<void>&& func) const {
			if (!func.empty()) {
				executor* exec = this->exec;

				loop->on_ready(fd, type, capture([exec](function<void>& func) {
					execute(exec, std::move(func));
				}, std::move(func)));
			}
		}

		void resolve(std::tuple<T...> args) const {
			resolve(typename make_index_sequence<sizeof...(T)>::type(), args);
		}

		void resolve(T... args) const {
			resolve(std::make_tuple(args...));
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
	private:
		function<void, context<T...>&> func;
	public:
		using tuple_type = std::tuple<T...>;

		future(const future&) = delete;

		future(future&& other) : func(std::move(other.func)) {
		}

		explicit future(T... args) {
			this->func = [args...](context<T...>& ctx) {
				ctx.resolve(args...);
			};
		}

		future(function<void, context<T...>&>&& func) : func(std::move(func)) {
		}

		future& operator=(future other) {
			std::swap(func, other.func);
			return *this;
		}

		void start(context<T...>&& ctx)&& {
			func(ctx);
		}

		void run(context<T...>&& ctx)&& {
			ctx.run(std::bind(std::move(func), std::move(ctx)));
		}

		template<class... U>
		future<U...> then(typename type_identity<function<future<U...>, T...>>::type&& func)&& {
			using func_type = typename type_identity<function<future<U...>, T...>>::type;

			return future<U...>(capture([](future<T...>& self, func_type& func, context<U...>& ctx) {
				std::move(self).start(ctx.template detach<T...>(capture([](func_type& func, context<U...>& ctx, T... args) {
					func(args...).start(std::move(ctx));
				}, std::move(func), std::move(ctx))));
			}, std::move(*this), std::move(func)));
		}
		
		template<class... U>
		future<U...> map_flat(typename type_identity<function<std::tuple<U...>, T...>>::type&& func)&& {
			using func_type = typename type_identity<function<std::tuple<U...>, T...>>::type;

			return future<U...>(capture([](future<T...>& self, func_type& func, context<U...>& ctx) {
				std::move(self).start(ctx.template detach<T...>(capture([](func_type& func, context<U...>& ctx, T... args) {
					ctx.resolve(func(args...));
				}, std::move(func), std::move(ctx))));
			}, std::move(*this), std::move(func)));
		}

		template<class U>
		future<U> map(typename type_identity<function<U, T...>>::type&& func)&& {
			using func_type = typename type_identity<function<U, T...>>::type;

			return future<U>(capture([](future<T...>& self, func_type& func, context<U>& ctx) {
				std::move(self).start(ctx.template detach<T...>(capture([](func_type& func, context<U>& ctx, T... args) {
					ctx.resolve(func(args...));
				}, std::move(func), std::move(ctx))));
			}, std::move(*this), std::move(func)));
		}

		future<tuple_type> tie()&& {
			return std::move(*this).template map<tuple_type>([](T... args) {
				return std::make_tuple(args...);
			});
		}
	};

	template<class... Future, std::size_t... I>
	future<typename Future::tuple_type...> all_impl(index_sequence<I...>, Future&&... futs) {
		struct all_state {
			std::tuple<typename Future::tuple_type...> result;
			context<typename Future::tuple_type...> ctx;
			std::atomic_size_t progress;

			all_state(context<typename Future::tuple_type...>&& ctx) : ctx(std::move(ctx)), progress(0) {
			}
		};

		return future<typename Future::tuple_type...>(
			capture([](Future&... futs, context<typename Future::tuple_type...>& ctx) {
				std::shared_ptr<all_state> state = std::make_shared<all_state>(std::move(ctx));

				std::make_tuple((std::move(futs).tie().run(
					ctx.template detach<typename Future::tuple_type>(
						[state](typename Future::tuple_type arg) {
							std::get<I>(state->result) = arg;

							if (++state->progress == sizeof...(Future)) {
								state->ctx.resolve(std::get<I>(state->result)...);
							}
						}
					)
				), 0)...);
			}, std::move(futs)...)
		);
	}

	template<class... Future>
	future<typename Future::tuple_type...> all(Future&&... futs) {
		return all_impl(typename make_index_sequence<sizeof...(Future)>::type(), std::move(futs)...);
	}

	template<class... Future, std::size_t... I>
	typename rename_tuple<decltype(std::tuple_cat(std::declval<typename Future::tuple_type>()...))>::template type<future> all_flat_impl(index_sequence<I...>, Future&&... futs) {
		using tuple_type = decltype(std::tuple_cat(std::declval<typename Future::tuple_type>()...));

		return all(std::move(futs)...).template map_flat<typename std::tuple_element<I, tuple_type>::type...>(
			[](typename Future::tuple_type... args) {
				return std::tuple_cat(args...);
			}
		);
	}

	template<class... Future>
	typename rename_tuple<decltype(std::tuple_cat(std::declval<typename Future::tuple_type>()...))>::template type<future> all_flat(Future&&... futs) {
		using tuple_type = decltype(std::tuple_cat(std::declval<typename Future::tuple_type>()...));

		return all_flat_impl(typename make_index_sequence<std::tuple_size<tuple_type>::value>::type(), std::move(futs)...);
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
