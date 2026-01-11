#include "TestUtils.hpp"

#include <Cory/Renderer/MappedCoherentDeviceBuffer.hpp>

#include <catch2/catch_test_macros.hpp>

#include <Cory/Base/Log.hpp>
#include <KDGpu/vulkan/vulkan_graphics_api.h>
#include <cstdint>

TEST_CASE("MappedCoherentDeviceBuffer basic allocation", "[Cory/Renderer]")
{
    Cory::testing::VulkanTester tester;
    auto &ctx = tester.ctx();

    Cory::MappedCoherentDeviceBuffer buffer{
        ctx,
        Cory::MappedCoherentDeviceBufferCreateInfo{
            .label = "MappedCoherentDeviceBuffer Test",
            .size = 1024,
            .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        },
    };

    REQUIRE(buffer.isValid());
    CHECK(buffer.size() == 1024);
    REQUIRE(buffer.mappedData() != nullptr);
    CHECK(buffer.deviceAddress() != 0);

    auto *data = static_cast<std::uint32_t *>(buffer.mappedData());
    data[0] = 0xDEADBEEF;
    CHECK(data[0] == 0xDEADBEEF);

    auto b = Cory::MappedCoherentDeviceBuffer(ctx,
                                              Cory::MappedCoherentDeviceBufferCreateInfo{
                                                  .label = "Test Buffer",
                                                  .size = 2ull * 1024 * 1024 * 1024, /* 2 GiB */
                                                  //.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                              });

    REQUIRE(b.isValid());
}
