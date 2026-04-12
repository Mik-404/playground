#pragma once

#include <tuple>

template <typename F> struct FunctionTraits;

template <typename R, typename... Args>
struct FunctionTraits<R(*)(Args...)> {
    using Pointer = R(*)(Args...);
    using RetType = R;
    using ArgTypes = std::tuple<Args...>;

    static constexpr std::size_t ArgCount = sizeof...(Args);

    template <std::size_t N>
    using NthArg = std::tuple_element_t<N, ArgTypes>;

    using FirstArg = NthArg<0>;
    using LastArg = NthArg<ArgCount - 1>;
};

template<typename R, typename... Args>
struct FunctionTraits<R(*)(Args...) noexcept> : FunctionTraits<R(*)(Args...)> {};
