#ifndef COBRA_UTIL_HH
#define COBRA_UTIL_HH

#include <tuple>
#include <memory>

namespace cobra {
	typedef std::tuple<> unit;

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
	struct rename_tuple {};

	template<class... T>
	struct rename_tuple<std::tuple<T...>> {
		template<template<class...> class Type>
		using type = Type<T...>;
	};

	template<class T, class... Args>
	std::unique_ptr<T> make_unique(Args&&... args) {
		return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
	}
}

#endif
