#include "cobra/exception.hh"

#include <cerrno>

namespace cobra {

	//TODO errno exception werkt wss niet echt goed..., gewoon een std::exception maken met strerror
	errno_exception::errno_exception(int errc) : std::system_error(errc, std::generic_category()) {}
	errno_exception::errno_exception() : errno_exception(errno) {}
}
