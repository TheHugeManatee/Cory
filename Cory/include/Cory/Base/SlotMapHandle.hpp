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
    [[nodiscard]] bool valid() const noexcept { return !free_ && index_ != INVALID_INDEX; }

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

/**
 * A typed version of the handle to ensure type safety
 * @tparam T        the type that the handle accesses - purely used for type safety
 * @tparam Friend   a friend class that is able to "dereference" the handle
 *
 * This version is intended to be used to hide the slot map and provide type safety.
 * A class can internally use a SlotMap to store objects, and hand out typed handles
 * to the outside. Users can then store and use the handle to be passed around, but are
 * not able to directly access the SlotMapHandle itself, preventing misuse by for example
 * passing a texture handle to an operation that expects a buffer handle.
 */
template <typename T, typename Friend> class PrivateTypedHandle {
  public:
    /// default initialization creates an invalid handle
    PrivateTypedHandle() = default;
    auto operator<=>(const PrivateTypedHandle &rhs) const = default;

    /**
     * check a handle for validity. Note: a valid handle does NOT imply that it
     * actually references an alive object in the slot map, use SlotMap::valid() for that!
     */
    [[nodiscard]] bool valid() const { return handle_.valid(); }
    explicit operator bool() const { return valid(); }

  private:
    friend Friend;
    friend struct std::hash<Cory::PrivateTypedHandle<T, Friend>>;
    friend struct fmt::formatter<Cory::PrivateTypedHandle<T, Friend>>;
    /* implicit */ PrivateTypedHandle(SlotMapHandle handle) // NOLINT
        : handle_{handle}
    {
    }
    /*implicit*/ operator SlotMapHandle() const noexcept // NOLINT
    {
        return handle_;
    }
    SlotMapHandle handle_{};
};

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
    return {old.index(), old.version() + 1, old.free_ > 0};
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
        if (h.valid()) { return fmt::format_to(ctx.out(), "{{{},{}}}", h.index(), h.version()); }

        return fmt::format_to(ctx.out(), "{{invalid}}");
    }
};

/// make SlotMapHandle hashable
template <> struct std::hash<Cory::SlotMapHandle> {
    std::size_t operator()(const Cory::SlotMapHandle &s) const noexcept
    {
        return Cory::hashCompose(0, s.valid(), s.index(), s.version());
    }
};

/// make PrivateTypedHandles formattable
template <typename T, typename F>
struct fmt::formatter<Cory::PrivateTypedHandle<T, F>> : public fmt::formatter<Cory::SlotMapHandle> {
    auto format(Cory::PrivateTypedHandle<T, F> h, format_context &ctx)
    {
        return fmt::formatter<Cory::SlotMapHandle>::format(h.handle_, ctx);
    }
};

// partial specialization needs to be in std, not sure if that's a good idea though..
namespace std {
template <typename T, typename F> struct hash<Cory::PrivateTypedHandle<T, F>> {
    std::size_t operator()(const Cory::PrivateTypedHandle<T, F> &s) const noexcept
    {
        return std::hash<Cory::SlotMapHandle>{}(s);
    }
};
} // namespace std