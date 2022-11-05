#pragma once

#include <Cory/Base/Math.hpp>

#include <fmt/core.h>

#include <cstdint>

namespace Cory {
/**
 * @brief a handle type that encodes an index and a version.
 *
 * Uses a dedicated bit to indicate whether the handle points to a value
 * that is free (this is an optimization, technically the free bit would be a property of the
 * object storage itself, but this seemed like the best place to encode it).
 */
class SlotMapHandle {
  public:
    static constexpr uint32_t INVALID_INDEX{0xFFFF'FFFF};

    /// default-constructed handle has invalid version and index
    SlotMapHandle();
    /// construct a handle with given index and version
    SlotMapHandle(uint32_t index, uint32_t version = 0, bool free = false);
    [[nodiscard]] uint32_t index() const noexcept { return index_; }
    [[nodiscard]] uint32_t version() const noexcept { return version_; }
    [[nodiscard]] bool alive() const noexcept { return !free_; }

    [[nodiscard]] static SlotMapHandle nextVersion(SlotMapHandle old);
    [[nodiscard]] static SlotMapHandle clearFreeBit(SlotMapHandle handle);
    [[nodiscard]] static SlotMapHandle setFreeBit(SlotMapHandle handle);
    auto operator<=>(const SlotMapHandle &rhs) const = default;

  private:
    uint32_t index_ : 32;
    uint32_t free_ : 1;
    uint32_t version_ : 31;
};
static_assert(sizeof(SlotMapHandle) == sizeof(uint64_t));
static_assert(std::copyable<SlotMapHandle> && std::movable<SlotMapHandle>);

inline SlotMapHandle::SlotMapHandle()
    : free_{1}
    , version_{0}
    , index_{INVALID_INDEX}
{
}
inline SlotMapHandle::SlotMapHandle(uint32_t index, uint32_t version, bool free)
    : free_{free ? 1u : 0u}
    , version_{version}
    , index_{index}
{
}
inline SlotMapHandle SlotMapHandle::nextVersion(SlotMapHandle old)
{
    return {old.index(), old.version() + 1};
}
inline SlotMapHandle SlotMapHandle::clearFreeBit(SlotMapHandle handle)
{
    return {handle.index(), handle.version(), false};
}
inline SlotMapHandle SlotMapHandle::setFreeBit(SlotMapHandle handle)
{
    return {handle.index(), handle.version(), true};
}
} // namespace Cory

/// make SlotMapHandle formattable
template <> struct fmt::formatter<Cory::SlotMapHandle> {
    template <typename ParseContext> constexpr auto parse(ParseContext &ctx) { return ctx.end(); }
    auto format(Cory::SlotMapHandle h, format_context &ctx)
    {
        return fmt::format_to(ctx.out(), "{}({}{})", h.index(), h.version(), h.alive() ? "" : "F");
    }
};

/// make SlotMapHandle hashable
template <> struct std::hash<Cory::SlotMapHandle> {
    std::size_t operator()(const Cory::SlotMapHandle &s) const noexcept
    {
        return Cory::hashCompose(0, s.alive(), s.index(), s.version());
    }
};