#ifndef COBRA_EXCEPTION_HH
#define COBRA_EXCEPTION_HH

#include <system_error>

namespace cobra {

	class errno_exception : public std::system_error {
	public:
		errno_exception(int errc);
		errno_exception();
	};
}

#endif
