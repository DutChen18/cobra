#include "cobra/exception.hh"

#include <cerrno>
#include <cstring>
#include <stdexcept>

namespace cobra {
	// TODO use std::make_error_code with std::system_error?
	// https://stackoverflow.com/questions/13950938/construct-stderror-code-from-errno-on-posix-and-getlasterror-on-windows
	errno_exception::errno_exception() : errno_exception(errno) {}
	errno_exception::errno_exception(int errc) : std::runtime_error(std::strerror(errc)), _errc(errc) {}
	timeout_exception::timeout_exception() : std::runtime_error("something timed out") {}
	parse_error::parse_error(const std::string& what) : std::runtime_error(what) {}
	parse_error::parse_error(const char* what) : std::runtime_error(what) {}
} // namespace cobra
