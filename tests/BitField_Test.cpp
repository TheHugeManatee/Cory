#include <catch2/catch_test_macros.hpp>

#include <Cory/Base/BitField.hpp>
#include <Cory/Base/FmtUtils.hpp>

#include <fmt/format.h>

#include <bit>

enum class BitValues : uint32_t {
    First = 0x0000'0001,
    Second = 0x0000'0002,
    Third = 0x0000'0004,
    Highest = 0x8000'0000
};
template <> struct magic_enum::customize::enum_range<BitValues> {
    static constexpr bool is_flags = true;
};

TEST_CASE("BitField", "[Cory/Base]")
{
    SECTION("Default-initialized state")
    {
        Cory::BitField<BitValues> bitfield;

        CHECK(bitfield.underlying_bits() == 0);
        CHECK_FALSE(bitfield.is_set(BitValues::First));
        CHECK_FALSE(bitfield.is_set(BitValues::Second));
        CHECK_FALSE(bitfield.is_set(BitValues::Third));
        CHECK_FALSE(bitfield.is_set(BitValues::Highest));
        CHECK(bitfield.set_bits() == std::vector<BitValues>{});
    }

    SECTION("Constexpr usage")
    {
        constexpr auto bitfield = Cory::BitField<BitValues>{}.set(BitValues::Third);

        static_assert(!bitfield.is_set(BitValues::First));
        static_assert(!bitfield.is_set(BitValues::Second));
        static_assert(bitfield.is_set(BitValues::Third));
        static_assert(!bitfield.is_set(BitValues::Highest));
    }

    SECTION("Setting and clearing bits")
    {
        Cory::BitField<BitValues> bitfield;

        // set one bit
        bitfield.set(BitValues::First);
        REQUIRE(bitfield.underlying_bits() == 0x0000'0001);
        CHECK(bitfield.is_set(BitValues::First));
        CHECK_FALSE(bitfield.is_set(BitValues::Second));
        CHECK_FALSE(bitfield.is_set(BitValues::Third));
        CHECK_FALSE(bitfield.is_set(BitValues::Highest));
        CHECK(bitfield.set_bits().size() == std::popcount(bitfield.underlying_bits()));
        CHECK(bitfield.set_bits() == std::vector{BitValues::First});

        // set another bit
        bitfield.set(BitValues::Third);
        REQUIRE(bitfield.underlying_bits() == (0x0000'0001 | 0x0000'0004));
        CHECK(bitfield.is_set(BitValues::First));
        CHECK_FALSE(bitfield.is_set(BitValues::Second));
        CHECK(bitfield.is_set(BitValues::Third));
        CHECK_FALSE(bitfield.is_set(BitValues::Highest));
        CHECK(bitfield.set_bits() == std::vector{BitValues::Third, BitValues::First});

        // clear an unset bit
        bitfield.clear(BitValues::Second);
        REQUIRE(bitfield.underlying_bits() == (0x0000'0001 | 0x0000'0004));
        CHECK(bitfield.is_set(BitValues::First));
        CHECK_FALSE(bitfield.is_set(BitValues::Second));
        CHECK(bitfield.is_set(BitValues::Third));
        CHECK_FALSE(bitfield.is_set(BitValues::Highest));
        CHECK(bitfield.set_bits() == std::vector{BitValues::Third, BitValues::First});

        // clear a set bit
        bitfield.clear(BitValues::First);
        REQUIRE(bitfield.underlying_bits() == 0x0000'0004);
        CHECK_FALSE(bitfield.is_set(BitValues::First));
        CHECK_FALSE(bitfield.is_set(BitValues::Second));
        CHECK(bitfield.is_set(BitValues::Third));
        CHECK_FALSE(bitfield.is_set(BitValues::Highest));
        CHECK(bitfield.set_bits() == std::vector{BitValues::Third});

        // set a bit that's already set
        bitfield.set(BitValues::Third);
        REQUIRE(bitfield.underlying_bits() == 0x0000'0004);
        CHECK_FALSE(bitfield.is_set(BitValues::First));
        CHECK_FALSE(bitfield.is_set(BitValues::Second));
        CHECK(bitfield.is_set(BitValues::Third));
        CHECK_FALSE(bitfield.is_set(BitValues::Highest));
        CHECK(bitfield.set_bits() == std::vector{BitValues::Third});

        // chaining
        bitfield.set(BitValues::First).set(BitValues::Highest).clear(BitValues::Third);
        REQUIRE(bitfield.underlying_bits() == (0x0000'0001 | 0x8000'0000));
        CHECK(bitfield.is_set(BitValues::First));
        CHECK_FALSE(bitfield.is_set(BitValues::Second));
        CHECK_FALSE(bitfield.is_set(BitValues::Third));
        CHECK(bitfield.is_set(BitValues::Highest));
        CHECK(bitfield.set_bits() == std::vector{BitValues::Highest, BitValues::First});

        // toggle a bit that was on
        bitfield.toggle(BitValues::First);
        REQUIRE(bitfield.underlying_bits() == 0x8000'0000);
        CHECK_FALSE(bitfield.is_set(BitValues::First));
        CHECK_FALSE(bitfield.is_set(BitValues::Second));
        CHECK_FALSE(bitfield.is_set(BitValues::Third));
        CHECK(bitfield.is_set(BitValues::Highest));
        CHECK(bitfield.set_bits() == std::vector{BitValues::Highest});

        // toggle a bit that was off
        bitfield.toggle(BitValues::Second);
        REQUIRE(bitfield.underlying_bits() == (0x8000'0000 | 0x0000'0002));
        CHECK_FALSE(bitfield.is_set(BitValues::First));
        CHECK(bitfield.is_set(BitValues::Second));
        CHECK_FALSE(bitfield.is_set(BitValues::Third));
        CHECK(bitfield.is_set(BitValues::Highest));
        CHECK(bitfield.set_bits() == std::vector{BitValues::Highest, BitValues::Second});
    }

    SECTION("Formatting")
    {
        Cory::BitField<BitValues> bitfield;

        auto formatted_empty = fmt::format("{}", bitfield);
        CHECK(formatted_empty == "0 (0x0)");

        bitfield.set(BitValues::First).set(BitValues::Third).set(BitValues::Highest);
        auto formatted_set_bits = fmt::format("{}", bitfield);
        CHECK(formatted_set_bits == "Highest | Third | First");
    }
}