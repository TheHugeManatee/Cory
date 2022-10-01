#include <Cory/Cory.hpp>

#include <Cory/Core/Log.hpp>

int main()
{
    Cory::Log::Init();

    CO_APP_INFO("Cory value is {}", Cory::test_function());
    CO_APP_INFO("Vulkan instance version is {}", Cory::queryVulkanInstanceVersion());

    Cory::playground_main();

}