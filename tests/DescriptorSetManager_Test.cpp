#include <Cory/Renderer/DescriptorSets.hpp>

#include "TestUtils.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Basic Usage")
{
    Cory::testing::VulkanTester t;

    Cory::DescriptorSets descriptorSetManager;

    GIVEN("A set of descriptor set options")
    {
        Gpu::ResourceBindingFlags bindless_flags;
        bindless_flags |= Gpu::ResourceBindingFlagBits::PartiallyBoundBit;
        bindless_flags |= Gpu::ResourceBindingFlagBits::UpdateAfterBindBit;
        auto createOptions = Gpu::BindGroupLayoutOptions{
            .label = "Default Bind Group Layout",
            .bindings =
                {
                    {
                        .binding = 0,
                        .count = 1,
                        .resourceType = Gpu::ResourceBindingType::UniformBuffer,
                        .shaderStages = Gpu::ShaderStageFlagBits::All,
                        .flags = bindless_flags,
                    },
                },
            .flags = KDGpu::BindGroupLayoutFlagBits::UpdateAfterBind};

        WHEN("Initializing the manager")
        {
            descriptorSetManager.init(t.ctx().device(), createOptions);

            THEN("It works") {}
        }
    }
}
