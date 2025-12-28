#include <Cory/Renderer/DescriptorSets.hpp>

#include "TestUtils.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Basic Usage")
{
    Cory::testing::VulkanTester t;

    Cory::DescriptorSets descriptorSetManager;

    GIVEN("A set of descriptor set options")
    {
        auto createOptions = Cory::DescriptorSetOptions{
            .label = "Default Bind Group Layout",
        };

        WHEN("Initializing the manager")
        {
            descriptorSetManager.init(t.ctx().device(), createOptions);

            THEN("It works") {}
        }
    }
}
