#include <catch2/catch_test_macros.hpp>

#include <Cory/Base/SignalTree.hpp>

TEST_CASE("SignalTree", "[Cory/Base]")
{
    SECTION("Initializes with a power of two")
    {
        CHECK_NOTHROW(Cory::SignalTree(2));
        CHECK_NOTHROW(Cory::SignalTree(8));
        CHECK_THROWS(Cory::SignalTree(7));
        CHECK_THROWS(Cory::SignalTree(123));
    }
}