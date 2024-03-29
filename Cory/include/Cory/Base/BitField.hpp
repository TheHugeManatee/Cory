#pragma once

#include <fmt/format.h>
#include <magic_enum.hpp>

#include <vector>

namespace Cory {

template <class T>
concept Enum = std::is_enum_v<T>;

constexpr auto to_underlying(Enum auto value)
{
    return static_cast<std::underlying_type_t<decltype(value)>>(value);
}

template <typename UnderlyingEnum>
    requires Enum<UnderlyingEnum>
class BitField {
  public:
    using UnderlyingType = std::underlying_type_t<UnderlyingEnum>;
    static constexpr int NUM_BITS = std::numeric_limits<UnderlyingType>::digits;

    constexpr BitField()
        : bits_{}
    {
    }
    constexpr /*implicit*/ BitField(UnderlyingEnum bits)
        : bits_{to_underlying(bits)}
    {
    }
    constexpr /*implicit*/ BitField(UnderlyingType bits)
        : bits_{bits}
    {
    }

    auto operator<=>(const BitField &rhs) const = default;

    /// access to the raw enum
    [[nodiscard]] constexpr UnderlyingEnum bits() const { return bits_; }
    /// access to the underlying bits
    [[nodiscard]] constexpr UnderlyingType underlying_bits() const { return to_underlying(bits_); }

    /// set a bit
    constexpr BitField &set(UnderlyingEnum bit)
    {
        bits_ = UnderlyingEnum(to_underlying(bits_) | to_underlying(bit));
        return *this;
    }

    /// clear a bit
    constexpr BitField &clear(UnderlyingEnum bit)
    {
        bits_ = UnderlyingEnum(to_underlying(bits_) & ~to_underlying(bit));
        return *this;
    }

    /// toggles a bit (sets it if it was unset, clears it if it was set)
    constexpr BitField &toggle(UnderlyingEnum bit)
    {
        bits_ = UnderlyingEnum(to_underlying(bits_) ^ to_underlying(bit));
        return *this;
    }

    /// query bit
    [[nodiscard]] constexpr bool is_set(UnderlyingEnum bit) const
    {
        return (to_underlying(bits_) & to_underlying(bit)) == to_underlying(bit);
    }

    /// returns a vector of single bits that are set
    [[nodiscard]] std::vector<UnderlyingEnum> set_bits() const
    {
        std::vector<UnderlyingEnum> setBits;

        for (unsigned int digit = NUM_BITS; digit > 0; --digit) {
            UnderlyingEnum bit{UnderlyingType(1) << (digit - 1)};
            if (is_set(bit)) { setBits.push_back(bit); }
        }
        return setBits;
    }

  private:
    UnderlyingEnum bits_;
};

} // namespace Cory

template <typename E>
struct fmt::formatter<Cory::BitField<E>, std::enable_if_t<std::is_enum_v<E>, char>>
    : fmt::formatter<std::string_view, char> {
    auto format(Cory::BitField<E> e, format_context &ctx) const
    {
        if (e.bits() == E{}) { return fmt::format_to(ctx.out(), "{}", E{}); }
        return fmt::format_to(ctx.out(), "{}", fmt::join(e.set_bits(), " | "));
    }
};