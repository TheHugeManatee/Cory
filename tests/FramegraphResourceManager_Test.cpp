#include <catch2/catch_test_macros.hpp>

#include "TestUtils.hpp"

#include <Cory/Framegraph/FramegraphResourceManager.hpp>

#include <KDGpu/buffer_options.h>
#include <KDGpu/texture_options.h>

using namespace Cory;

TEST_CASE("Framegraph resource manager tracks textures and buffers",
          "[Cory/Framegraph/FramegraphResourceManager]")
{
    testing::VulkanTester tester;
    FramegraphResourceManager resources(tester.ctx());
    auto &device = tester.ctx().device();

    SECTION("Textures")
    {
        const TextureInfo texInfo{
            .name = "TEX_Test",
            .size = {64, 32, 1},
            .format = TextureFormat::R8G8B8A8_SRGB,
            .usage = Gpu::TextureUsageFlagBits::SampledBit,
            .sampleCount = Gpu::SampleCountFlagBits::Samples1Bit,
        };

        auto texHandle = resources.declareTexture(texInfo);
        CHECK(resources.info(texHandle).name == texInfo.name);
        CHECK(resources.state(texHandle).status == TextureMemoryStatus::Virtual);

        resources.allocate(std::vector<FramegraphTextureHandle>{texHandle});
        CHECK(resources.state(texHandle).status == TextureMemoryStatus::Allocated);
        CHECK(resources.image(texHandle).isValid());
        CHECK(resources.imageView(texHandle).isValid());


        Texture externalTexture = device.createTexture(Gpu::TextureOptions{
            .label = "TEX_External (IMG)",
            .type = Gpu::TextureType::TextureType2D,
            .format = TextureFormat::R8G8B8A8_SRGB,
            .extent = {16, 16, 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = Gpu::SampleCountFlagBits::Samples1Bit,
            .usage = Gpu::TextureUsageFlagBits::ColorAttachmentBit |
                     Gpu::TextureUsageFlagBits::SampledBit,
            .memoryUsage = Gpu::MemoryUsage::GpuOnly,
        });

        TextureView externalView = externalTexture.createView(Gpu::TextureViewOptions{
            .label = "TEX_External (VIEW)",
            .viewType = Gpu::ViewType::ViewType2D,
            .format = TextureFormat::R8G8B8A8_SRGB,
        });

        auto externalTexHandle =
            resources.registerExternal(TextureInfo{.name = "TEX_External",
                                                   .size = {16, 16, 1},
                                                   .format = TextureFormat::R8G8B8A8_SRGB,
                                                   .usage = Gpu::TextureUsageFlagBits::SampledBit},
                                       Sync::AccessType::None,
                                       externalTexture,
                                       externalView);

        CHECK(resources.state(externalTexHandle).status == TextureMemoryStatus::External);
        CHECK(resources.image(externalTexHandle) == externalTexture.handle());
        CHECK(resources.imageView(externalTexHandle) == externalView.handle());
    }
    SECTION("Buffers")
    {
        const BufferInfo bufferInfo{
            .name = "BUF_Test",
            .size = 256u,
            .usage = Gpu::BufferUsageFlagBits::StorageBufferBit,
            .memoryUsage = Gpu::MemoryUsage::GpuOnly,
        };

        auto bufferHandle = resources.declareBuffer(bufferInfo);
        CHECK(resources.info(bufferHandle).name == bufferInfo.name);
        CHECK(resources.state(bufferHandle).status == BufferMemoryStatus::Virtual);

        resources.allocate(std::vector<FramegraphBufferHandle>{bufferHandle});
        CHECK(resources.state(bufferHandle).status == BufferMemoryStatus::Allocated);
        CHECK(resources.buffer(bufferHandle).isValid());

        Gpu::Buffer externalBuffer = device.createBuffer(Gpu::BufferOptions{
            .label = "BUF_External",
            .size = 128u,
            .usage = Gpu::BufferUsageFlagBits::StorageBufferBit,
            .memoryUsage = Gpu::MemoryUsage::GpuOnly,
        });

        auto externalBufferHandle = resources.registerExternal(
            BufferInfo{.name = "BUF_External",
                       .size = 128u,
                       .usage = Gpu::BufferUsageFlagBits::StorageBufferBit,
                       .memoryUsage = Gpu::MemoryUsage::GpuOnly},
            Sync::AccessType::None,
            externalBuffer);

        CHECK(resources.state(externalBufferHandle).status == BufferMemoryStatus::External);
        CHECK(resources.buffer(externalBufferHandle) == externalBuffer.handle());
    }
    // Cleanup
    resources.clear();
}
