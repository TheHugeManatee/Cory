#pragma once

#include <magic_enum.hpp>
#include <fmt/core.h>

namespace Cory {

// a base class type to prevent copies
struct NoCopy {
    NoCopy() = default;
    NoCopy(const NoCopy &) = delete;
    NoCopy & operator=(const NoCopy &) = delete;
};

// a base class type to prevent moves
struct NoMove {
    NoMove() = default;
    NoMove(NoMove &&) = delete;
};


}