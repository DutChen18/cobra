#ifndef COBRA_FUTURE_HH
#define COBRA_FUTURE_HH

#include "cobra/executor.hh"
#include "cobra/event_loop.hh"
#include "cobra/util.hh"

#include <functional>
#include <atomic>

namespace cobra {
	template<class... T>
	class context {
	private:
		executor* exec;
		event_loop *loop;
		std::function<void(T... args)> func;
	public:
		context(executor* exec, event_loop* loop, std::function<void(T... args)> func) {
			this->exec = exec;
			this->loop = loop;
			this->func = func;
		}

		void resolve(T... args) const {
			exec->exec([this, args...]() {
				func(args...);
			});
		}

		executor& get_exec() const {
			return *exec;
		}

		event_loop& get_loop() const {
			return *loop;
		}

		template<class... U>
		context<U...> with(typename type_identity<std::function<void(U... args)>>::type func) const {
			return context<U...>(exec, loop, func);
		}

		context<> detach() const {
			return with([]() {});
		}
	};

	template<class... T>
	class future {
		std::function<void(const context<T...>& ctx)> func;
	public:
		using tuple_type = std::tuple<T...>;

		future(T... args) {
			this->func = [args...](const context<T...>& ctx) {
				ctx.resolve(args...);
			};
		}

		future(std::function<void(const context<T...>& ctx)> func) {
			this->func = func;
		}

		void start(const context<T...>& ctx) const {
			func(ctx);
		}

		template<class... U>
		future<U...> then(typename type_identity<std::function<future<U...>(T... args)>>::type func) const {
			future<T...> self = *this;

			return future<U...>([self, func](const context<U...>& ctx) {
				self.start(ctx.template with<T...>([ctx, func](T... args) {
					future<U...> fut = func(args...);

					fut.start(ctx);
				}));
			});
		}

		future<tuple_type> tie() const {
			return then<tuple_type>([](T... args) {
				return future<tuple_type>(std::make_tuple(args...));
			});
		}
	};

	template<class... Future, std::size_t... I>
	future<typename Future::tuple_type...> all_impl(index_sequence<I...>, Future... futs) {
		return future<typename Future::tuple_type...>(
			[futs...](const context<typename Future::tuple_type...>& ctx) {
				std::tuple<typename Future::tuple_type...> result;
				std::atomic_size_t progress(0);

				std::make_tuple((futs.tie().start(
					ctx.template with<typename Future::tuple_type>(
						[ctx, &result, &progress](typename Future::tuple_type arg) {
							std::get<I>(result) = arg;

							if (++progress == sizeof...(Future)) {
								ctx.resolve(std::get<I>(result)...);
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

	template<class T>
	future<T> run(const context<T>& ctx, future<T> fut) {
	}

	inline future<> async_while(std::function<future<bool>()> func) {
		std::function<future<>(bool)> lambda;

		lambda = [lambda, func](bool cond) {
			if (cond) {
				return func().then(lambda);
			} else {
				return future<>();
			}
		};

		return func().then(lambda);
	}
}

#endif
