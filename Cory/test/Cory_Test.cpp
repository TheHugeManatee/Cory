#include <catch2/catch.hpp>

#include <Cory/Cory.hpp>
#include <fmt/format.h>

TEST_CASE("Library Function works", "[Cory]")
{
  fmt::print("Vulkan version: {}", Cory::queryVulkanInstanceVersion());
}
