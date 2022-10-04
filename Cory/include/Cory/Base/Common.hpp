#pragma once

#include <fmt/core.h>
#include <magic_enum.hpp>

namespace Cory {

// a base class type to prevent copies
struct NoCopy {
    NoCopy() = default;
    NoCopy(const NoCopy &) = delete;
    NoCopy &operator=(const NoCopy &) = delete;
    NoCopy(NoCopy &&) = default;
    NoCopy &operator=(NoCopy &&) = default;
};

// a base class type to prevent moves
struct NoMove {
    NoMove() = default;
    NoMove(const NoMove &) = default;
    NoMove &operator=(const NoMove &) = default;
    NoMove(NoMove &&) = delete;
    NoMove &operator=(NoMove &&) = delete;
};

} // namespace Cory

// declare an enum as a bitfield - helps with automatic stringification with lib{fmt}
#define DECLARE_ENUM_BITFIELD(type)                                                                \
    template <> struct magic_enum::customize::enum_range<type> {                                   \
        static constexpr bool is_flags = true;                                                     \
    };