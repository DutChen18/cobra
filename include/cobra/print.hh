#ifndef COBRA_PRINT_HH
#define COBRA_PRINT_HH

#include <format>
#include <iostream>
#include <cstdint>

namespace cobra {
	template <class... Args>
	std::ostream& print(std::ostream& os, std::format_string<Args...> fmt, Args&&... args) {
		return os << std::format(fmt, std::forward<Args>(args)...);
	}

	template <class... Args>
	std::ostream& println(std::ostream& os, std::format_string<Args...> fmt, Args&&... args) {
		return os << std::format(fmt, std::forward<Args>(args)...) << std::endl;
	}
	
	template <class... Args>
	std::ostream& print(std::format_string<Args...> fmt, Args&&... args) {
		return print(std::cout, fmt, std::forward<Args>(args)...);
	}

	template <class... Args>
	std::ostream& println(std::format_string<Args...> fmt, Args&&... args) {
		return println(std::cout, fmt, std::forward<Args>(args)...);
	}
	
	template <class... Args>
	std::ostream& eprint(std::format_string<Args...> fmt, Args&&... args) {
		return print(std::cerr, fmt, std::forward<Args>(args)...);
	}

	template <class... Args>
	std::ostream& eprintln(std::format_string<Args...> fmt, Args&&... args) {
		return println(std::cerr, fmt, std::forward<Args>(args)...);
	}

	namespace color {
		using attr_t = std::uint16_t;

		constexpr attr_t attr_bold = 0x0001;
		constexpr attr_t attr_faint = 0x0002;
		constexpr attr_t attr_italic = 0x0004;
		constexpr attr_t attr_underline = 0x0008;
		constexpr attr_t attr_slow_blink = 0x0010;
		constexpr attr_t attr_rapid_blink = 0x0020;
		constexpr attr_t attr_invert = 0x0040;
		constexpr attr_t attr_hide = 0x0080;
		constexpr attr_t attr_strike = 0x0100;

		class control {
			attr_t _attr_set;
			attr_t _attr_clr;

		public:
			control& operator|=(const control& other) {
				_attr_set = (_attr_set & ~other._attr_clr) | other._attr_set;
				_attr_clr = (_attr_clr & ~other._attr_set) | other._attr_clr;
				return *this;
			}

			friend control operator|(control lhs, const control& rhs) {
				return lhs |= rhs;
			}
		};
	}
}

#endif
