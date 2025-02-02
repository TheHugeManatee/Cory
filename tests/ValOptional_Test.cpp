#include <catch2/catch_test_macros.hpp>

#include <Cory/Base/ValOptional.hpp>

using TestOptional = Cory::ValOptional<int, -1000>;

TEST_CASE("ValueOptional", "Cory/Base/ValueOptional")
{
    static_assert(TestOptional::InvalidValue == -1000);

    TestOptional opt1{-10};
    TestOptional opt2{43};
    REQUIRE(opt1.has_value());
    REQUIRE(opt1);
    REQUIRE(opt1.value() == -10);
    REQUIRE(*opt1 == -10);
    REQUIRE(opt1.value_or(1) == -10);

    REQUIRE(opt1 != opt2);
    REQUIRE(opt1 == TestOptional{-10});

    TestOptional empty_opt1;
    REQUIRE(!empty_opt1.has_value());
    REQUIRE(!empty_opt1);
    REQUIRE_THROWS(empty_opt1.value());
    REQUIRE(*empty_opt1 == TestOptional::InvalidValue);
    REQUIRE(empty_opt1.value_or(1) == 1);

    REQUIRE(opt1 != empty_opt1);
    REQUIRE(empty_opt1 == TestOptional{});

    SECTION("and_then")
    {
        auto op = [](int v) { return float(v * v); };

        auto then1 = opt1.and_then(op);
        REQUIRE(then1.has_value());
        REQUIRE(then1.value() == 100.0f);

        auto then2 = empty_opt1.and_then(op);
        REQUIRE(!then2.has_value());
    }
}