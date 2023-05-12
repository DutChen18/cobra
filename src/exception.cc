#include "cobra/exception.hh"

#include <cerrno>
#include <signal.h>

#define TRAP_ON_ERROR false

namespace cobra {

	//TODO errno exception werkt wss niet echt goed..., gewoon een std::exception maken met strerror
	errno_exception::errno_exception(int errc) : std::system_error(errc, std::generic_category()) {
		if (TRAP_ON_ERROR) {
			kill(getpid(), SIGTRAP);
		}
	}

	errno_exception::errno_exception() : errno_exception(errno) {
		if (TRAP_ON_ERROR) {
			kill(getpid(), SIGTRAP);
		}
	}
}
