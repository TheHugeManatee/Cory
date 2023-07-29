#pragma once

#include <concepts>
#include <type_traits>

namespace Cory {

template <typename T, typename... Args>
concept CallableWith = requires(T t, Args... args) {
    {
        t(args...)
    } -> std::same_as<void>;
};

}