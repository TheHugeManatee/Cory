#pragma once

#include <Cory/Base/BitField.hpp>
#include <Cory/Base/SlotMapHandle.hpp>

#include <fmt/core.h>
#include <gsl/gsl>
#include <magic_enum.hpp>

#include <concepts>

namespace Cory {
template <typename T> using Span = gsl::span<T>;

// forward declarations
template <typename StoredType> class SlotMap;
template <int64_t RECORD_HISTORY_SIZE = 64> class ProfilerRecord;

class Profiler;
class ScopeTimer;
class LapTimer;
class ResourceLocator;

// a base class type to prevent copies
struct NoCopy {
    NoCopy() = default;
    NoCopy(const NoCopy &) = delete;
    NoCopy &operator=(const NoCopy &) = delete;
    NoCopy(NoCopy &&) = default;
    NoCopy &operator=(NoCopy &&) = default;
};
static_assert(!std::copyable<NoCopy>);

// a base class type to prevent moves
struct NoMove {
    NoMove() = default;
    NoMove(const NoMove &) = default;
    NoMove &operator=(const NoMove &) = default;
    NoMove(NoMove &&) = delete;
    NoMove &operator=(NoMove &&) = delete;
};
static_assert(!std::movable<NoMove>);

} // namespace Cory

// declare an enum as a bitfield - helps with automatic stringification with lib{fmt}
#define DECLARE_ENUM_BITFIELD(type)                                                                \
    template <> struct magic_enum::customize::enum_range<type> {                                   \
        static constexpr bool is_flags = true;                                                     \
    };