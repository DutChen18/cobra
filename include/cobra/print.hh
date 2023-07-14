#ifndef COBRA_PRINT_HH
#define COBRA_PRINT_HH

#include "cobra/asyncio/stream.hh"

#include <format>
#include <iostream>
#include <cstdint>

namespace cobra {
	template <class... Args>
	task<void> print(ostream_reference os, std::format_string<Args...> fmt, Args&&... args) {
		std::string str = std::format(fmt, std::forward<Args>(args)...);
		co_await os.write_all(str.data(), str.size());
	}

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

	namespace term {
		using attr_t = std::uint16_t;
		using channel_t = std::uint8_t;

		constexpr attr_t attr_reset = 0x0001;
		constexpr attr_t attr_bold = 0x0002;
		constexpr attr_t attr_faint = 0x0004;
		constexpr attr_t attr_italic = 0x0008;
		constexpr attr_t attr_underline = 0x0010;
		constexpr attr_t attr_slow_blink = 0x0020;
		constexpr attr_t attr_rapid_blink = 0x0040;
		constexpr attr_t attr_invert = 0x0080;
		constexpr attr_t attr_hide = 0x0100;
		constexpr attr_t attr_strike = 0x0200;

		constexpr channel_t col_black = 0;
		constexpr channel_t col_red = 1;
		constexpr channel_t col_green = 2;
		constexpr channel_t col_yellow = 3;
		constexpr channel_t col_blue = 4;
		constexpr channel_t col_magenta = 5;
		constexpr channel_t col_cyan = 6;
		constexpr channel_t col_white = 7;

		enum class color_mode : std::uint8_t {
			int_3,
			int_8,
			int_24,
			reset,
			keep,
		};

		class color {
			color_mode _mode;
			channel_t _val[3];

		public:
			constexpr color() : color(color_mode::keep) {
			}

			constexpr color(color_mode mode) : _mode(mode) {
			}

			constexpr color(color_mode mode, channel_t val) : _mode(mode), _val { val, val, val } {
			}

			constexpr color(color_mode mode, channel_t red, channel_t green, channel_t blue) : _mode(mode), _val { red, green, blue } {
			}

			constexpr color& operator|=(const color& other) {
				return *this = other._mode == color_mode::keep ? *this : other;
			}

			friend constexpr color operator|(color lhs, const color& rhs) {
				return lhs |= rhs;
			}

			constexpr color_mode mode() const {
				return _mode;
			}

			constexpr channel_t val() const {
				return _val[0];
			}

			constexpr channel_t red() const {
				return _val[0];
			}

			constexpr channel_t green() const {
				return _val[1];
			}

			constexpr channel_t blue() const {
				return _val[2];
			}
		};

		class control {
			attr_t _attr_set;
			attr_t _attr_clear;
			color _fg;
			color _bg;

		public:
			constexpr control() : control(0, 0) {
			}

			constexpr control(attr_t attr_set, attr_t attr_clear) : _attr_set(attr_set), _attr_clear(attr_clear) {
			}

			constexpr control(color fg, color bg) : _attr_set(0), _attr_clear(0), _fg(fg), _bg(bg) {
			}

			constexpr control& operator|=(const control& other) {
				if (other._attr_set & attr_reset) {
					return *this = other;
				} else {
					_attr_set = (_attr_set & ~other._attr_clear) | other._attr_set;
					_attr_clear = (_attr_clear & ~other._attr_set) | other._attr_clear;
					_fg |= other._fg;
					_bg |= other._bg;
					return *this;
				}
			}

			friend constexpr control operator|(control lhs, const control& rhs) {
				return lhs |= rhs;
			}

			constexpr attr_t attr_set() const {
				return _attr_set;
			}

			constexpr attr_t attr_clear() const {
				return _attr_clear;
			}

			constexpr color fg() const {
				return _fg;
			}

			constexpr color bg() const {
				return _bg;
			}
		};

		constexpr control set(attr_t attr) {
			return control(attr, 0);
		}

		constexpr control set_bold() {
			return control(attr_bold, 0);
		}

		constexpr control set_faint() {
			return control(attr_faint, 0);
		}

		constexpr control set_italic() {
			return control(attr_italic, 0);
		}

		constexpr control set_underline() {
			return control(attr_underline, 0);
		}

		constexpr control set_slow_blink() {
			return control(attr_slow_blink, 0);
		}

		constexpr control set_rapid_blink() {
			return control(attr_rapid_blink, 0);
		}

		constexpr control set_invert() {
			return control(attr_invert, 0);
		}

		constexpr control set_hide() {
			return control(attr_hide, 0);
		}

		constexpr control set_strike() {
			return control(attr_strike, 0);
		}

		constexpr control clear(attr_t attr) {
			return control(0, attr);
		}
		
		constexpr control clear_bold() {
			return control(0, attr_bold);
		}

		constexpr control clear_faint() {
			return control(0, attr_faint);
		}

		constexpr control clear_italic() {
			return control(0, attr_italic);
		}

		constexpr control clear_underline() {
			return control(0, attr_underline);
		}

		constexpr control clear_slow_blink() {
			return control(0, attr_slow_blink);
		}

		constexpr control clear_rapid_blink() {
			return control(0, attr_rapid_blink);
		}

		constexpr control clear_invert() {
			return control(0, attr_invert);
		}

		constexpr control clear_hide() {
			return control(0, attr_hide);
		}

		constexpr control clear_strike() {
			return control(0, attr_strike);
		}

		constexpr control reset() {
			return control(attr_reset, 0);
		}

		constexpr control fg_3(channel_t val) {
			return control(color(color_mode::int_3, val), color());
		}

		constexpr control fg_8(channel_t val) {
			return control(color(color_mode::int_8, val), color());
		}

		constexpr control fg_24(channel_t red, channel_t green, channel_t blue) {
			return control(color(color_mode::int_24, red, green, blue), color());
		}

		constexpr control fg_24(std::uint32_t val) {
			return fg_24(val >> 16 & 0xFF, val >> 8 & 0xFF, val >> 0 & 0xFF);
		}

		constexpr control fg_black() {
			return fg_3(col_black);
		}

		constexpr control fg_red() {
			return fg_3(col_red);
		}

		constexpr control fg_green() {
			return fg_3(col_green);
		}

		constexpr control fg_yellow() {
			return fg_3(col_yellow);
		}

		constexpr control fg_blue() {
			return fg_3(col_blue);
		}

		constexpr control fg_magenta() {
			return fg_3(col_magenta);
		}

		constexpr control fg_cyan() {
			return fg_3(col_cyan);
		}

		constexpr control fg_white() {
			return fg_3(col_white);
		}

		constexpr control fg_reset() {
			return control(color(color_mode::reset), color());
		}

		constexpr control bg_3(channel_t val) {
			return control(color(), color(color_mode::int_3, val));
		}

		constexpr control bg_8(channel_t val) {
			return control(color(), color(color_mode::int_8, val));
		}

		constexpr control bg_24(channel_t red, channel_t green, channel_t blue) {
			return control(color(), color(color_mode::int_24, red, green, blue));
		}

		constexpr control bg_24(std::uint32_t val) {
			return bg_24(val >> 16 & 0xFF, val >> 8 & 0xFF, val >> 0 & 0xFF);
		}

		constexpr control bg_black() {
			return bg_3(col_black);
		}

		constexpr control bg_red() {
			return bg_3(col_red);
		}

		constexpr control bg_green() {
			return bg_3(col_green);
		}

		constexpr control bg_yellow() {
			return bg_3(col_yellow);
		}

		constexpr control bg_blue() {
			return bg_3(col_blue);
		}

		constexpr control bg_magenta() {
			return bg_3(col_magenta);
		}

		constexpr control bg_cyan() {
			return bg_3(col_cyan);
		}

		constexpr control bg_white() {
			return bg_3(col_white);
		}

		constexpr control bg_reset() {
			return control(color(), color(color_mode::reset));
		}

		constexpr control col_3(channel_t fg_val, channel_t bg_val) {
			return fg_3(fg_val) | bg_3(bg_val);
		}

		constexpr control col_8(channel_t fg_val, channel_t bg_val) {
			return fg_8(fg_val) | bg_8(bg_val);
		}

		constexpr control col_24(channel_t fg_red, channel_t fg_green, channel_t fg_blue, channel_t bg_red, channel_t bg_green, channel_t bg_blue) {
			return fg_24(fg_red, fg_green, fg_blue) | bg_24(bg_red, bg_green, bg_blue);
		}

		constexpr control col_24(std::uint32_t fg_val, std::uint32_t bg_val) {
			return fg_24(fg_val) | bg_24(bg_val);
		}

		constexpr control col_reset() {
			return fg_reset() | bg_reset();
		}
	}
}

template <>
class std::formatter<cobra::term::control> {
	template <class... Args>
	static bool format(std::format_context& fc, bool w, std::format_string<Args...> fmt, Args&&... args) {
		fc.advance_to(std::format_to(fc.out(), "{}", w ? ";" : "\033["));
		fc.advance_to(std::format_to(fc.out(), fmt, std::forward<Args>(args)...));
		return true;
	}

	static bool format_attr(std::format_context& fc, bool w, cobra::term::control ctrl, cobra::term::attr_t attr, int t, int f) {
		if (ctrl.attr_set() & attr) {
			return format(fc, w, "{}", t);
		} else if (ctrl.attr_clear() & attr) {
			return format(fc, w, "{}", f);
		} else {
			return w;
		}
	}

	static bool format_col(std::format_context& fc, bool w, cobra::term::color col, int i) {
		if (col.mode() == cobra::term::color_mode::int_3) {
			return format(fc, w, "{}", i + col.val());
		} else if (col.mode() == cobra::term::color_mode::int_8) {
			return format(fc, w, "{};5;{}", i + 8, col.val());
		} else if (col.mode() == cobra::term::color_mode::int_24) {
			return format(fc, w, "{};2;{};{};{}", i + 8, col.red(), col.green(), col.blue());
		} else if (col.mode() == cobra::term::color_mode::reset) {
			return format(fc, w, "{}", i + 9);
		} else {
			return w;
		}
	}

public:
	constexpr auto parse(std::format_parse_context& fpc) {
		return fpc.begin();
	}

	auto format(cobra::term::control ctrl, std::format_context& fc) const {
		bool w = false;

		if (ctrl.attr_set() & cobra::term::attr_reset) {
			w = format(fc, w, "0");
		}

		w = format_attr(fc, w, ctrl, cobra::term::attr_bold, 1, 22);
		w = format_attr(fc, w, ctrl, cobra::term::attr_faint, 2, 22);
		w = format_attr(fc, w, ctrl, cobra::term::attr_italic, 3, 23);
		w = format_attr(fc, w, ctrl, cobra::term::attr_underline, 4, 24);
		w = format_attr(fc, w, ctrl, cobra::term::attr_slow_blink, 5, 25);
		w = format_attr(fc, w, ctrl, cobra::term::attr_rapid_blink, 6, 25);
		w = format_attr(fc, w, ctrl, cobra::term::attr_invert, 7, 27);
		w = format_attr(fc, w, ctrl, cobra::term::attr_hide, 8, 28);
		w = format_attr(fc, w, ctrl, cobra::term::attr_strike, 9, 29);
		w = format_col(fc, w, ctrl.fg(), 30);
		w = format_col(fc, w, ctrl.bg(), 40);

		return std::format_to(fc.out(), "{}", w ? "m" : "");
	}
};

#endif
