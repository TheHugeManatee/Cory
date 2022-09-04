#include <catch2/catch.hpp>

#include <Cory/Cory.hpp>

TEST_CASE("Library Function works", "[Cory]")
{
  REQUIRE(Cory::test_function() == 42);
}
