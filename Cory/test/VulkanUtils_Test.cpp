#include <catch2/catch_test_macros.hpp>

#include <Magnum/Vk/Vulkan.h>

#include <Cory/Base/Log.hpp>
 #include <Cory/Renderer/VulkanUtils.hpp>


TEST_CASE("VulkanUtils::PNextChain", "[VulkanUtils]")
{
    SECTION("Empty chain")
    {
        auto make_empty_chain = []() {
            Cory::PNextChain chain;

            return chain;
        };
        Cory::PNextChain chain = make_empty_chain();

        CHECK(chain.head() == nullptr);
    }

    SECTION("Chain with things")
    {
        auto make_chain = []() {
            Cory::PNextChain chain;

            // synchronization2
            chain.prepend(VkPhysicalDeviceSynchronization2Features{
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES,
                .synchronization2 = VK_TRUE});

            // dynamic_rendering
            chain.prepend(VkPhysicalDeviceDynamicRenderingFeatures{
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
                .dynamicRendering = VK_TRUE,
            });

            // indexing_features (required for bindless)
            chain.prepend(VkPhysicalDeviceDescriptorIndexingFeatures{
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES,
                .descriptorBindingPartiallyBound = VK_TRUE,
                .runtimeDescriptorArray = VK_TRUE});

            return chain;
        };

        const Cory::PNextChain chain = make_chain();

        REQUIRE(chain.size() == 3);
        REQUIRE(chain.head() != nullptr);

        // NOLINTNEXTLINE
        auto *firstEntry =
            reinterpret_cast<VkPhysicalDeviceDescriptorIndexingFeatures *>(chain.head());
        REQUIRE(firstEntry->sType ==
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES);
        REQUIRE(firstEntry->pNext != nullptr);
        fmt::print("First entry = 0x{}\n", fmt::ptr(firstEntry));

        // NOLINTNEXTLINE
        auto *secondEntry =
            reinterpret_cast<VkPhysicalDeviceDynamicRenderingFeatures *>(firstEntry->pNext);
        REQUIRE(secondEntry->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES);
        REQUIRE(secondEntry->pNext != nullptr);
        fmt::print("Second entry = 0x{}\n", fmt::ptr(secondEntry));

        // NOLINTNEXTLINE
        auto *thirdEntry =
            reinterpret_cast<VkPhysicalDeviceSynchronization2Features *>(secondEntry->pNext);
        REQUIRE(thirdEntry->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES);
        REQUIRE(thirdEntry->pNext == nullptr);
        fmt::print("Third entry = 0x{}\n", fmt::ptr(thirdEntry));

    }
}