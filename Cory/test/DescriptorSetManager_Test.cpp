#include <Cory/Renderer/DescriptorSetManager.hpp>

#include "TestUtils.hpp"

#include <catch2/catch_test_macros.hpp>

#include <Magnum/Vk/DescriptorPoolCreateInfo.h>
#include <Magnum/Vk/DescriptorSetLayoutCreateInfo.h>
#include <Magnum/Vk/DescriptorType.h>

TEST_CASE("Basic Usage")
{
    namespace testing = Cory::testing;
    namespace Vk = Magnum::Vk;

    testing::VulkanTester t;

    Cory::DescriptorSetManager descriptorSetManager;

    GIVEN("An existing descriptor pool and layout")
    {
        Vk::DescriptorPool pool{
            t.ctx().device(),
            Vk::DescriptorPoolCreateInfo{8, // max descriptor sets
                                         {
                                             {Vk::DescriptorType::UniformBuffer, 3 * 3},
                                             {Vk::DescriptorType::CombinedImageSampler, 24},
                                             {Vk::DescriptorType::CombinedImageSampler, 24},
                                         }}};
        // default layout currently only has a single uniform buffer and eight images and buffers
        Vk::DescriptorSetLayoutCreateInfo layout{
            {{0, Vk::DescriptorType::UniformBuffer}},
            {{1, Vk::DescriptorType::CombinedImageSampler, 8}},
            {{2, Vk::DescriptorType::StorageBuffer, 8}},
        };

        WHEN("Initializing the manager")
        {
            descriptorSetManager.init(t.ctx().device(), t.ctx().resources(), std::move(layout), 3);

            THEN("It works") {}
        }
    }
}