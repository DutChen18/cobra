#ifndef COBRA_LOCALE_HH
#define COBRA_LOCALE_HH

#include <unordered_map>
#include <string>

namespace cobra {
	extern std::unordered_map<std::string, std::unordered_map<std::string, std::string>> locale;
}

#endif
