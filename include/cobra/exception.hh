#ifndef COBRA_EXCEPTION_HH
#define COBRA_EXCEPTION_HH

#include <stdexcept>

namespace cobra {

	class errno_exception : public std::runtime_error {
	public:
		errno_exception();
		errno_exception(int errc);
	};

	class timeout_exception : public std::runtime_error {
	public:
		timeout_exception();
	};
} // namespace cobra

#endif
