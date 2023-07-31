#ifndef COBRA_EXCEPTION_HH
#define COBRA_EXCEPTION_HH

#include <stdexcept>

namespace cobra {

	class errno_exception : public std::runtime_error {
		int _errc;

	public:
		errno_exception();
		errno_exception(int errc);

		inline int errc() const { return _errc; }
	};

	class timeout_exception : public std::runtime_error {
	public:
		timeout_exception();
	};

	class parse_error : public std::runtime_error {
	public:
		parse_error(const std::string& what);
		parse_error(const char* what);
	};

} // namespace cobra

#endif
