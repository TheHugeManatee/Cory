#pragma once

#include <fmt/format.h>
#include <magic_enum.hpp>

namespace Cory {

template <class T>
concept Enum = std::is_enum_v<T>;

auto to_underlying(Enum auto value)
{
    return static_cast<std::underlying_type_t<decltype(value)>>(value);
}

template <typename UnderlyingEnum> class BitField {
  public:
    using UnderlyingType = std::underlying_type_t<UnderlyingEnum>;
    static constexpr int NUM_BITS = std::numeric_limits<UnderlyingType>::digits;

    BitField()
        : bits_{}
    {
    }
    BitField(UnderlyingEnum bits)
        : bits_{to_underlying(bits)}
    {
    }
    BitField(UnderlyingType bits)
        : bits_{bits}
    {
    }

    /// access to the raw enum
    UnderlyingEnum bits() const { return UnderlyingEnum{bits_}; }
    /// access to the underlying bits
    UnderlyingType underlying_bits() const { return bits_; }

    /// set a bit
    BitField &set(UnderlyingEnum bit)
    {
        bits_ |= to_underlying(bit);
        return *this;
    }

    /// clear a bit
    BitField &clear(UnderlyingEnum bit)
    {
        bits_ &= ~to_underlying(bit);
        return *this;
    }

    /// toggles a bit (sets it if it was unset, clears it if it was set)
    BitField &toggle(UnderlyingEnum bit)
    {
        bits_ ^= to_underlying(bit);
        return *this;
    }

    /// query bit
    bool is_set(UnderlyingEnum bit) const
    {
        return (bits_ & to_underlying(bit)) == to_underlying(bit);
    }

    /// returns a vector of single bits that are set
    std::vector<UnderlyingEnum> set_bits() const
    {
        std::vector<UnderlyingEnum> setBits;

        for (unsigned int digit = NUM_BITS; digit > 0; --digit) {
            UnderlyingEnum bit{UnderlyingType(1u << (digit - 1))};
            if (is_set(bit)) { setBits.push_back(bit); }
        }
        return setBits;
    }

  private:
    UnderlyingType bits_;
};

} // namespace Cory

template <typename E>
struct fmt::formatter<Cory::BitField<E>, std::enable_if_t<std::is_enum_v<E>, char>>
    : fmt::formatter<std::string_view, char> {
    auto format(Cory::BitField<E> e, format_context &ctx) const
    {
        return fmt::format_to(
            ctx.out(), "0x{:X} [{}]", e.underlying_bits(), fmt::join(e.set_bits(), " | "));
    }
};