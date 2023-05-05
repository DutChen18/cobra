#ifndef COBRA_UTIL_HH
#define COBRA_UTIL_HH

#include <tuple>

namespace cobra {
	template<class T>
	struct type_identity {
		using type = T;
	};

	template<std::size_t... I>
	struct index_sequence {
	};

	template<std::size_t N, std::size_t... I>
	struct make_index_sequence : make_index_sequence<N - 1, N - 1, I...> {
	};

	template<std::size_t... I>
	struct make_index_sequence<0, I...> {
		using type = index_sequence<I...>;
	};

	template<class T>
	struct flat_tuple {
		using type = std::tuple<T>;

		static type flat(T value) {
			return std::make_tuple(value);
		}
	};

	template<class... T>
	struct flat_tuple<std::tuple<T...>> {
		using type = decltype(std::tuple_cat(std::declval<typename flat_tuple<T>::type>()...));

		static type flat(T... values) {
			return std::tuple_cat(flat_tuple<std::tuple<T>>::flat(values)...);
		}
	};

	template<class T>
	struct rename_tuple {};

	template<class... T>
	struct rename_tuple<std::tuple<T...>> {
		template<template<class...> class Type>
		using type = Type<T...>;
	};
}

#endif
