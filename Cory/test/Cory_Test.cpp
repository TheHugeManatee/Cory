#include <catch2/catch_test_macros.hpp>

#include <Cory/Cory.hpp>
#include <fmt/format.h>

TEST_CASE("Library Function works", "[Cory]")
{
    // dump instance info
    Cory::dumpInstanceInformation();
}
