#include <Cory/Cory.hpp>

#include <spdlog/spdlog.h>

int main()
{
    spdlog::info("Cory value is {}", Cory::test_function());
    spdlog::info("Vulkan instance version is {}", Cory::queryVulkanInstanceVersion());
}