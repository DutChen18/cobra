#ifndef COBRA_TEXT_HH
#define COBRA_TEXT_HH

#include "cobra/print.hh"

#include <format>
#include <string>

#ifdef COBRA_USE_GETTEXT
#include <locale>
#include <mutex>
#else
#include "cobra/locale.hh"
#endif

#define COBRA_TEXT(...) cobra::translate(__VA_ARGS__)

namespace cobra {
	template <class... Args>
	std::string translate(std::string fmt, Args&&... args) {
#ifdef COBRA_USE_GETTEXT
		static std::once_flag flag;
		static std::locale loc;
		static const std::messages<char>* facet;
		static std::messages<char>::catalog cat;

		std::call_once(flag, []() {
			// loc = std::locale("nl_NL.UTF-8");
			loc = std::locale("");
			facet = &std::use_facet<std::messages<char>>(loc);
			cat = facet->open("cobra", loc);
		});

		fmt = facet->get(cat, 0, 0, std::move(fmt));
#else
		if (const char* env = std::getenv("LC_MESSAGES")) {
			fmt = locale.at(env).at(fmt);
		}
#endif

		return std::vformat(fmt, std::make_format_args(std::forward<Args>(args)...));
	}
} // namespace cobra

#endif
