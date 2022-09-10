#include <Cory/Cory.hpp>

#include <Magnum/Vk/Version.h>

#include <fmt/core.h>

namespace Cory {
int test_function() { return 42; }
std::string queryVulkanInstanceVersion()
{
    auto version = Magnum::Vk::enumerateInstanceVersion();
    return fmt::format(
        "{}.{}.{}", versionMajor(version), versionMinor(version), versionPatch(version));
}
} // namespace Cory