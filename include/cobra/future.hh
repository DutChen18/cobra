#ifndef COBRA_FUTURE_HH
#define COBRA_FUTURE_HH

#include "cobra/context.hh"
#include "cobra/util.hh"
#include "cobra/function.hh"
#include "cobra/optional.hh"

#include <atomic>
#include <functional>
#include <stdexcept>
#include <iostream>

namespace cobra {
	using future_error = std::runtime_error;

	template<class T>
	using future_result = result<T, future_error>;

	template<class T>
	using future_func = function<void, future_result<T>>;

	template<class T>
	class future;
	
	template<class T>
	future<T> resolve(T&& arg) {
		return future<T>(capture([](T& arg, context&, future_func<T>& resolve) {
			resolve(ok<T, future_error>(std::forward<T>(arg)));
		}, std::forward<T>(arg)));
	}

	template<class T>
	future<T> reject(const std::exception& ex) {
		return future<T>(capture([](future_error& ex, context&, future_func<T>& resolve) {
			resolve(err<T, future_error>(std::move(ex)));
		}, future_error(ex.what())));
	}

	template<class T>
	class future {
	private:
		function<void, context&, future_func<T>&> func;

		static void default_resolve(future_result<T> result) {
			if (result.err()) {
				std::cerr << result.err()->what() << std::endl;
			}
		}
	public:
		using return_type = T;

		future() = default;
		future(const future&) = delete;
		future(future&&) = default;

		// TODO: warning if future never awaited

		future(function<void, context&, future_func<T>&>&& func) : func(std::move(func)) {
		}

		future& operator=(future other) {
			std::swap(func, other.func);
			return *this;
		}

		void start_now(context& ctx, future_func<T>&& resolve = default_resolve)&& {
			func(ctx, resolve);
		}

		void start_later(context& ctx, future_func<T>&& resolve = default_resolve)&& {
			ctx.execute(capture(std::move(func), ctx, std::move(resolve)));
		}

		template<class U>
		future<U> and_then(function<future<U>, T>&& func)&& {
			using func_type = function<future<U>, T>;

			return future<U>(capture([](future& self, func_type& func, context& ctx, future_func<U>& resolve) {
				std::move(self).start_now(ctx, capture([&ctx](func_type& func, future_func<U>& resolve, future_result<T> result) {
					if (result) {
						future<U> ret;

						try {
							ret = func(std::move(*result));
						} catch (const std::exception& ex) {
							resolve(err<U, future_error>(ex.what()));
							return;
						}

						std::move(ret).start_now(ctx, std::move(resolve));
					} else {
						resolve(err<U, future_error>(*result.err()));
					}
				}, std::move(func), std::move(resolve)));
			}, std::move(*this), std::move(func)));
		}

		future or_else(function<future, future_error>&& func)&& {
			using func_type = function<future, T>;

			return future(capture([](future& self, func_type& func, context& ctx, future_func<T>& resolve) {
				std::move(self).start_now(ctx, capture([&ctx](func_type& func, future_func<T>& resolve, future_result<T> result) {
					if (result) {
						resolve(std::move(result));
					} else {
						future ret;

						try {
							ret = func(*result.err());
						} catch (const std::exception& ex) {
							resolve(err<T, future_error>(ex.what()));
							return;
						}

						std::move(ret).start_now(ctx, std::move(resolve));
					}
				}, std::move(func), std::move(resolve)));
			}, std::move(*this), std::move(func)));
		}
	};
	
	template<class T>
	future<unit> spawn(future<T>&& fut) {
		return future<T>(capture([](future<T>& fut, context& ctx, future_func<unit>& resolve) {
			std::move(fut).start_later(ctx);
			resolve(ok<unit, future_error>());
		}, std::move(fut)));
	}

	template<class... Future, std::size_t... Index>
	future<std::tuple<typename Future::return_type...>> all(index_sequence<Index...>, Future&&... futs) {
		using tuple_type = std::tuple<typename Future::return_type...>;

		struct all_state {
			tuple_type result;
			future_func<tuple_type> resolve;
			std::atomic_size_t progress;
			std::atomic_bool error;

			all_state(future_func<tuple_type>&& resolve) : resolve(std::move(resolve)), progress(0), error(false) {
			}
		};

		return future<tuple_type>(capture([](Future&... futs, context& ctx, future_func<tuple_type>& resolve) {
			std::shared_ptr<all_state> state = std::make_shared<all_state>(std::move(resolve));

			std::make_tuple((std::move(futs).start_later(ctx, [state](future_result<typename Future::return_type> r) {
				if (r) {
					std::get<Index>(state->result) = *r;

					if (++state->progress == sizeof...(Future)) {
						state->resolve(ok<tuple_type, future_error>(std::move(state->result)));
					}
				} else if (!state->error.exchange(true)) {
					state->resolve(err<tuple_type, future_error>(*r.err()));
				}
			}), 0)...);
		}, std::move(futs)...));
	}

	template<class... Future>
	future<std::tuple<typename Future::return_type...>> all(Future&&... futs) {
		return all(typename make_index_sequence<sizeof...(Future)>::type(), std::move(futs)...);
	}

	template<class T>
	future<T> async_while(function<future<optional<T>>>&& func) {
		return func().template and_then<T>(capture([](function<future<optional<T>>>& func, optional<T> result) {
			return result ? resolve(std::move(*result)) : async_while(std::move(func));
		}, std::move(func)));
	}
}

#endif
