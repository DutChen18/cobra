#include "cobra/exception.hh"

#include <cerrno>

namespace cobra {

	errno_exception::errno_exception(int errc) : std::system_error(errc, std::generic_category()) {}
	errno_exception::errno_exception() : errno_exception(errno) {}
}
