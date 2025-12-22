
#include <catch2/catch_test_macros.hpp>

#include <Cory/Base/Function.hpp>

TEST_CASE("Function", "[Cory/Base]")
{
    static_assert(std::movable<Cory::Function<int(int)>>);
    static_assert(std::copyable<Cory::Function<int(int)>>);

    SECTION("Invoking non-const functions")
    {
        Cory::Function<int(int)> f = [](int x) { return x + 1; };
        REQUIRE(f(1) == 2);

        Cory::Function<int(int)> g = [i = 0](int x) mutable { return ++i + x; };
        REQUIRE(g(1) == 2);
        REQUIRE(g(1) == 3);

        Cory::Function<int(int)> h;
        CHECK_FALSE(h);
    }


    SECTION("Invoking const functions")
    {
        const Cory::Function<int(int)> f = [](int x) { return x + 1; };
        REQUIRE(f(1) == 2);

        const Cory::Function<int(int)> g = [i = 0](int x) mutable { return ++i + x; };
        REQUIRE(g(1) == 2);
        REQUIRE(g(1) == 3);

        const Cory::Function<int(int)> h;
        CHECK_FALSE(h);
    }
}