#ifndef COBRA_TEXT_HH
#define COBRA_TEXT_HH

#include "cobra/print.hh"

#include <string>
#include <format>
#include <locale>
#include <mutex>

#define COBRA_TEXT(...) cobra::translate(__VA_ARGS__)

namespace cobra {
	template<class... Args>
	std::string translate(std::string fmt, Args&&... args) {
		static std::once_flag flag;
		static std::locale loc;
		static const std::messages<char>* facet;
		static std::messages<char>::catalog cat;

		std::call_once(flag, []() {
			//loc = std::locale("nl_NL.UTF-8");
			loc = std::locale("ja_JP.UTF-8");
			facet = &std::use_facet<std::messages<char>>(loc);
			cat = facet->open("cobra", loc);
		});

		fmt = facet->get(cat, 0, 0, std::move(fmt));

		return std::vformat(fmt, std::make_format_args(std::forward<Args>(args)...));
	}
}

#endif
