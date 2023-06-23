#include "cobra/exception.hh"

#include <cerrno>
#include <cstring>

namespace cobra {
	//TODO use std::make_error_code with std::system_error?
	//https://stackoverflow.com/questions/13950938/construct-stderror-code-from-errno-on-posix-and-getlasterror-on-windows
	errno_exception::errno_exception() : errno_exception(errno) {}
	errno_exception::errno_exception(int errc) : std::runtime_error(std::strerror(errc)) {}
	timeout_exception::timeout_exception() : std::runtime_error("something timed out") {}
}
