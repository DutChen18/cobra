#ifndef COBRA_ARGS_HH
#define COBRA_ARGS_HH

#include "cobra/print.hh"

#include <string_view>
#include <ranges>
#include <format>
#include <vector>

namespace cobra {
	class argument_error : public std::runtime_error {
	public:
		argument_error(const std::string& what) : std::runtime_error(what) {
		}

		argument_error(const char* what) : std::runtime_error(what) {
		}
	};

	template <class Result>
	class parse_conv {
	public:
		using result_type = Result;

		template <std::input_iterator I, std::sentinel_for<I> S>
		result_type convert(I& begin, S end, const char* name) const {
			if (begin == end) {
				throw argument_error(std::format("argument missing parameter: {}", name));
			}

			return static_cast<Result>(*begin++);
		}
	};

	template <class Result>
	class store_conv {
		Result _value;

	public:
		using result_type = Result;

		store_conv(Result value) : _value(std::move(value)) {
		}

		template <std::input_iterator I, std::sentinel_for<I> S>
		result_type convert(I& begin, S end, const char* name) const {
			(void) begin, (void) end, (void) name;

			return _value;
		}
	};

	class argument_helper {
		struct named_argument_help {
			std::string short_name;
			std::string long_name;
			std::string help;
		};

		struct positional_argument_help {
			std::string name;
			std::string help;
		};

		std::vector<named_argument_help> _named_help;
		std::vector<positional_argument_help> _positional_help;
		std::size_t _positional_arguments = 0;

	public:
		inline void named_argument(const char* short_name, const char* long_name, const char* help) {
			std::string short_name_str = short_name ? std::format("-{}", short_name) : "";
			std::string long_name_str = long_name ? std::format("--{}", long_name) : "";
			std::string help_str = help ? help : "";

			_named_help.push_back({ short_name_str, long_name_str, help_str });
		}

		inline void positional_argument(bool required, const char* name, const char* help) {
			std::string name_str_tmp = name ? name : std::format("arg_{}", _positional_arguments++);
			std::string name_str = required ? name_str_tmp : std::format("[{}]", name_str_tmp);
			std::string help_str = help ? help : "";

			_positional_help.push_back({ name_str, help_str });
		}

		inline void finish() {
			std::size_t short_name_size = 0;
			std::size_t long_name_size = 0;
			std::size_t name_size = 0;

			for (const named_argument_help& help : _named_help) {
				if (help.short_name.size() > short_name_size) {
					short_name_size = help.short_name.size();
				}
				
				if (help.long_name.size() > long_name_size) {
					long_name_size = help.long_name.size();
				}
			}

			for (const positional_argument_help& help : _positional_help) {
				if (help.name.size() > name_size ) {
					name_size = help.name.size();
				}
			}

			name_size = std::max(long_name_size + short_name_size + 1, name_size);
			long_name_size = name_size - short_name_size - 1;
			
			for (const named_argument_help& help : _named_help) {
				if (short_name_size != 0) {
					eprint("{:{}} ", help.short_name, short_name_size);
				}

				if (long_name_size != 0) {
					eprint("{:{}} ", help.long_name, long_name_size);
				}
				
				eprintln(" {}", help.help);
			}

			for (const positional_argument_help& help : _positional_help) {
				eprint("{:{}} ", help.name, name_size);
				eprintln(" {}", help.help);
			}
		}
	};

	template <class Result>
	class argument_base {
	public:
		using result_type = Result;

		struct state_type {
			state_type(const argument_base& argument) {
				(void) argument;
			}
		};

		void program_name(result_type& result, state_type& state, std::string_view str) const {
			(void) result, (void) state, (void) str;
		}

		template <std::input_iterator I, std::sentinel_for<I> S>
		bool long_argument(result_type& result, state_type& state, std::string_view arg, I& begin, S end) const {
			(void) result, (void) state, (void) arg, (void) begin, (void) end;
			return false;
		}

		template <std::input_iterator I, std::sentinel_for<I> S>
		bool short_argument(result_type& result, state_type& state, char arg, I& begin, S end) const {
			(void) result, (void) state, (void) arg, (void) begin, (void) end;
			return false;
		}

		bool positional_argument(result_type& result, state_type& state, std::string_view str) const {
			(void) result, (void) state, (void) str;
			return false;
		}

		void reset(result_type& result, state_type& state) const {
			(void) result, (void) state;
		}

		void validate(result_type& result, state_type& state) const {
			(void) result, (void) state;
		}

		template <class Helper>
		void get_help(Helper& helper) const {
			(void) helper;
		}
	};

	template <class Result>
	class program_name_arg : public argument_base<Result> {
		std::string Result::* _dest;

	public:
		using typename argument_base<Result>::result_type;
		using typename argument_base<Result>::state_type;

		program_name_arg(std::string Result::* dest) : _dest(dest) {
		}

		void program_name(result_type& result, state_type& state, std::string_view str) const {
			(void) state;

			result.*_dest = str;
		}
	};

	template <class Result, class Convert>
	class positional_arg : public argument_base<Result> {
		typename Convert::result_type Result::* _dest;
		Convert _convert;
		bool _required = false;
		const char* _name;
		const char* _help;

	public:
		using typename argument_base<Result>::result_type;
		
		struct state_type : argument_base<Result>::state_type {
			bool defined = false;
			
			state_type(const positional_arg& argument) : argument_base<Result>::state_type(argument) {
			}
		};

		positional_arg(typename Convert::result_type Result::* dest, Convert&& convert, bool required, const char* name, const char* help) : _dest(dest), _convert(std::move(convert)), _required(required), _name(name), _help(help) {
		}

		bool positional_argument(result_type& result, state_type& state, std::string_view str) const {
			if (std::exchange(state.defined, true)) {
				return false;
			}

			std::string_view* begin = &str;
			std::string_view* end = begin + 1;
			result.*_dest = _convert.convert(begin, end, _name);

			if (begin != end) {
				throw argument_error(std::format("failed to consume positional argument: {}", _name));
			}

			return true;
		}

		void validate(result_type& result, state_type& state) const {
			(void) result;

			if (!state.defined && _required) {
				throw argument_error(std::format("missing required position argument: {}", _name));
			}
		}

		template <class Helper>
		void get_help(Helper& helper) const {
			helper.positional_argument(_required, _name, _help);
		}
	};

	template <class Base, class Result>
	class named_argument_base : public argument_base<Result> {
		const char* _short_name;
		const char* _long_name;
		const char* _help;

	public:
		using typename argument_base<Result>::result_type;
		using typename argument_base<Result>::state_type;

		named_argument_base(const char* short_name, const char* long_name, const char* help) : _short_name(short_name), _long_name(long_name), _help(help) {
		}

		template <std::input_iterator I, std::sentinel_for<I> S>
		bool long_argument(result_type& result, state_type& state, std::string_view arg, I& begin, S end) const {
			if (_long_name && arg == _long_name) {
				static_cast<const Base*>(this)->parse_argument(result, static_cast<typename Base::state_type&>(state), begin, end, _long_name);

				return true;
			}

			return false;
		}

		template <std::input_iterator I, std::sentinel_for<I> S>
		bool short_argument(result_type& result, state_type& state, char arg, I& begin, S end) const {
			if (_short_name && arg == *_short_name) {
				static_cast<const Base*>(this)->parse_argument(result, static_cast<typename Base::state_type&>(state), begin, end, _short_name);

				return true;
			}

			return false;
		}

		template <class Helper>
		void get_help(Helper& helper) const {
			helper.named_argument(_short_name, _long_name, _help);
		}
	};

	template <class Result, class Convert>
	class argument_arg : public named_argument_base<argument_arg<Result, Convert>, Result> {
		using base = named_argument_base<argument_arg<Result, Convert>, Result>;

		typename Convert::result_type Result::* _dest;
		Convert _convert;

	public:
		using typename base::result_type;

		struct state_type : base::state_type {
			bool defined = false;
			
			state_type(const argument_arg& argument) : base::state_type(argument) {
			}
		};

		argument_arg(typename Convert::result_type Result::* dest, Convert&& convert, const char* short_name, const char* long_name, const char* help) : base(short_name, long_name, help), _dest(dest), _convert(std::move(convert)) {
		}

		template <std::input_iterator I, std::sentinel_for<I> S>
		void parse_argument(result_type& result, state_type& state, I& begin, S end, const char* name) const {
			if (std::exchange(state.defined, true)) {
				throw argument_error(std::format("duplicate argument: {}", name));
			}

			result.*_dest = _convert.convert(begin, end, name);
		}
	};
	
	template <class Base, class Argument>
	class argument_parser_chain;

	template <class Base, class Result>
	class argument_parser_base {
	public:
		using result_type = Result;

		auto add_program_name(std::string Result::* dest)&& {
			return argument_parser_chain(std::move(*static_cast<Base*>(this)), program_name_arg(dest));
		}

		template <class T>
		auto add_argument(T Result::* dest, const char* short_name, const char* long_name, const char* help)&& {
			return argument_parser_chain(std::move(*static_cast<Base*>(this)), argument_arg(dest, parse_conv<T>(), short_name, long_name, help));
		}

		template <class T>
		auto add_flag(T Result::* dest, T value, const char* short_name, const char* long_name, const char* help)&& {
			return argument_parser_chain(std::move(*static_cast<Base*>(this)), argument_arg(dest, store_conv(std::move(value)), short_name, long_name, help));
		}

		template <class T>
		auto add_positional(T Result::* dest, bool required, const char* name, const char* help)&& {
			return argument_parser_chain(std::move(*static_cast<Base*>(this)), positional_arg(dest, parse_conv<T>(), required, name, help));
		}

		template <std::input_iterator I, std::sentinel_for<I> S>
		result_type parse(I begin, S end) const {
			return parse_args(*static_cast<const Base*>(this), begin, end);
		}

		void help() const {
			return show_help(*static_cast<const Base*>(this));
		}
	};

	template <class Result>
	class argument_parser : public argument_parser_base<argument_parser<Result>, Result> {
	public:
		using typename argument_parser_base<argument_parser<Result>, Result>::result_type;

		struct state_type {
			state_type(const argument_parser& parser) {
				(void) parser;
			}
		};

		void program_name(result_type& result, state_type& state, std::string_view str) const {
			(void) result, (void) state, (void) str;
		}

		template <std::input_iterator I, std::sentinel_for<I> S>
		bool long_argument(result_type& result, state_type& state, std::string_view arg, I& begin, S end) const {
			(void) result, (void) state, (void) arg, (void) begin, (void) end;
			return false;
		}

		template <std::input_iterator I, std::sentinel_for<I> S>
		bool short_argument(result_type& result, state_type& state, char arg, I& begin, S end) const {
			(void) result, (void) state, (void) arg, (void) begin, (void) end;
			return false;
		}

		bool positional_argument(result_type& result, state_type& state, std::string_view str) const {
			(void) result, (void) state, (void) str;
			return false;
		}

		void reset(result_type& result, state_type& state) const {
			(void) result, (void) state;
		}

		void validate(result_type& result, state_type& state) const {
			(void) result, (void) state;
		}

		template <class Helper>
		void get_help(Helper& helper) const {
			(void) helper;
		}
	};

	template <class Base, class Argument>
	class argument_parser_chain : public argument_parser_base<argument_parser_chain<Base, Argument>, typename Base::result_type> {
		Base _base;
		Argument _arg;

	public:
		using typename argument_parser_base<argument_parser_chain<Base, Argument>, typename Base::result_type>::result_type;

		struct state_type {
			typename Base::state_type _base_state;
			typename Argument::state_type _arg_state;

			state_type(const argument_parser_chain& parser) : _base_state(parser._base), _arg_state(parser._arg) {
			}
		};

		argument_parser_chain(Base&& base, Argument&& arg) : _base(std::move(base)), _arg(std::move(arg)) {
		}

		void program_name(result_type& result, state_type& state, std::string_view str) const {
			_base.program_name(result, state._base_state, str);
			_arg.program_name(result, state._arg_state, str);
		}

		template <std::input_iterator I, std::sentinel_for<I> S>
		bool long_argument(result_type& result, state_type& state, std::string_view arg, I& begin, S end) const {
			if (_base.long_argument(result, state._base_state, arg, begin, end)) {
				return true;
			} else {
				return _arg.long_argument(result, state._arg_state, arg, begin, end);
			}
		}

		template <std::input_iterator I, std::sentinel_for<I> S>
		bool short_argument(result_type& result, state_type& state, char arg, I& begin, S end) const {
			if (_base.short_argument(result, state._base_state, arg, begin, end)) {
				return true;
			} else {
				return _arg.short_argument(result, state._arg_state, arg, begin, end);
			}
		}

		bool positional_argument(result_type& result, state_type& state, std::string_view str) const {
			if (_base.positional_argument(result, state._base_state, str)) {
				return true;
			} else {
				return _arg.positional_argument(result, state._arg_state, str);
			}
		}

		void reset(result_type& result, state_type& state) const {
			_base.reset(result, state._base_state);
			_arg.reset(result, state._arg_state);
		}

		void validate(result_type& result, state_type& state) const {
			_base.validate(result, state._base_state);
			_arg.validate(result, state._arg_state);
		}

		template <class Helper>
		void get_help(Helper& helper) const {
			_base.get_help(helper);
			_arg.get_help(helper);
		}
	};

	template <class Parser, std::input_iterator I, std::sentinel_for<I> S>
	bool parse_arg(const Parser& parser, typename Parser::result_type& result, typename Parser::state_type& state, std::string_view arg, I& begin, S end) {
		if (arg.starts_with("--")) {
			if (!parser.long_argument(result, state, arg.substr(2), begin, end)) {
				throw argument_error(std::format("bad argument: {}", arg));
			}
		} else if (arg.length() > 1) {
			for (char ch : arg.substr(1)) {
				if (!parser.short_argument(result, state, ch, begin, end)) {
					throw argument_error(std::format("bad argument: -{}", ch));
				}
			}
		} else {
			return false;
		}

		return true;
	}

	template <class Parser, std::input_iterator I, std::sentinel_for<I> S>
	typename Parser::result_type parse_args(const Parser& parser, I begin, S end) {
		typename Parser::result_type result;
		typename Parser::state_type state(parser);

		parser.reset(result, state);

		bool allow_flags = true;

		for (I it = begin; it != end;) {
			if (it == begin) {
				parser.program_name(result, state, static_cast<std::string_view>(*it++));
				continue;
			}

			std::string_view str = static_cast<std::string_view>(*it++);

			if (allow_flags) {
				if (str == "--") {
					allow_flags = false;
					continue;
				} else if (str.starts_with("-") && str.length() > 1) {
					std::size_t eq = str.find('=');
					bool has_arg;

					if (eq == std::string_view::npos) {
						has_arg = parse_arg(parser, result, state, str, it, end);
					} else {
						std::string_view arg = str.substr(eq + 1);
						std::string_view* arg_begin = &arg;
						std::string_view* arg_end = arg_begin + 1;

						has_arg = parse_arg(parser, result, state, str.substr(0, eq), arg_begin, arg_end);

						if (arg_begin != arg_end) {
							if (has_arg) {
								throw argument_error(std::format("unused parameter: {}", arg));
							} else {
								throw argument_error(std::format("bad argument: {}", str));
							}
						}
					}

					if (has_arg) {
						continue;
					}
				}
			}

			if (!parser.positional_argument(result, state, str)) {
				throw argument_error("too many positional arguments");
			}
		}

		parser.validate(result, state);

		return result;
	}

	template <class Parser>
	void show_help(const Parser& parser) {
		argument_helper helper;

		parser.get_help(helper);
		helper.finish();
	}
}

#endif
