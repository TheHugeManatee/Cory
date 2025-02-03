
#include <catch2/catch_test_macros.hpp>

#include <Cory/Base/Function.hpp>

TEST_CASE("Function", "[Cory/Base]")
{
    Cory::Function<int(int)> f = [](int x) { return x + 1; };

    REQUIRE(f(1) == 2);

    Cory::Function<int(int)> g = [i = 0](int x) mutable { return ++i + x; };
    REQUIRE(g(1) == 2);
    REQUIRE(g(1) == 3);
}