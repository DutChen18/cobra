#include "cobra/memory.hh"
#include "cobra/async.hh"

#include <iostream>

namespace cobra {
	class maybe_task {
	private:
		shared_ptr<task> t;
	public:
		maybe_task() {
		}

		maybe_task(shared_ptr<task> t) {
			this->t = t;
		}
	};
}

#define COBRA_CORO(Name, Arg, Types, Code) \
class Name { \
public: \
	Types \
	typedef Name current_ctx_t; \
	::cobra::maybe_task run(Arg arg) { \
		Code \
		return ::cobra::maybe_task(); \
	} \
};

#define COBRA_THEN(Expr, Arg, Types, Code) \
typedef current_ctx_t old_ctx_t; \
class functor { \
public: \
	Types; \
	old_ctx_t ctx; \
	typedef functor current_ctx_t; \
	functor(old_ctx_t ctx) : ctx(ctx) {} \
	::cobra::maybe_task run(Arg arg) { \
		Code \
		return ::cobra::maybe_task(); \
	} \
}; \
return ::cobra::functor_task<functor>(Expr, *this);

COBRA_CORO(add_one, int, int result;, {
	result = arg + 1;
})

COBRA_CORO(add_two, int, int result;, {
	COBRA_THEN(add_one(arg), int,, {
		COBRA_THEN(add_one(arg), int,, {
			ctx.ctx.result = arg;
		})
	})
})

int main() {
	std::cout << "Hello world" << std::endl;
}
