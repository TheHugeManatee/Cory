#include "TestUtils.hpp"

#include <catch2/catch_test_macros.hpp>

#include <Cory/Renderer/DescriptorSets.hpp>
#include <Cory/Renderer/ShaderBindingContext.hpp>
#include <KDGpu/texture_options.h>

TEST_CASE("Shader binding context: Memory bump allocation", "[ShaderBindingContext]")
{
    using namespace Cory;

    testing::VulkanTester tester;

    DescriptorSets descriptorSets;

    ShaderBindingContext bindingContext(tester.ctx().device(), descriptorSets, 0, 1024 * 1024);

    using OneKiB = std::array<uint8_t, 1024>;

    auto a1 = bindingContext.alloc<OneKiB>();
    CHECK(a1.size == 1);
    CHECK(a1.cpu != nullptr);
    CHECK(a1.gpu != 0);

    auto a2 = bindingContext.alloc<OneKiB>(2);
    CHECK(a2.size == 2);
    CHECK(a2.cpu != nullptr);
    CHECK(a2.gpu != 0);

    // Allocations grow
    REQUIRE(a2.gpu > a1.gpu);
    CHECK(a2.gpu - a1.gpu == 1024);
    CHECK((uint8_t *)a2.cpu - (uint8_t *)a1.cpu == 1024);

    CHECK_THROWS(bindingContext.alloc<OneKiB>(1023));

    bindingContext.reset();
    CHECK_NOTHROW(bindingContext.alloc<OneKiB>(1023));
}

TEST_CASE("Shader binding context: Texture 2D allocation", "[ShaderBindingContext]")
{
    using namespace Cory;

    testing::VulkanTester tester;

    DescriptorSets descriptorSets;

    ShaderBindingContext bindingContext(tester.ctx().device(), descriptorSets, 0, 1024 * 1024);

    auto tex = tester.ctx().device().createTexture(Gpu::TextureOptions{
        .label = "Test Texture",
        .type = Gpu::TextureType::TextureType2D,
        .format = Gpu::Format::B8G8R8A8_SRGB,
        .extent{4, 4, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .usage = Gpu::TextureUsageFlagBits::SampledBit,
        .memoryUsage = Gpu::MemoryUsage::GpuOnly,
    });
    auto view = tex.createView(Gpu::TextureViewOptions{
        .label = "Test Texture View",
    });

    TextureHeapIndex index1 = bindingContext.bindTexture2D(view);
    TextureHeapIndex index2 = bindingContext.bindTexture2D(view);

    CHECK(index2 > index1);

    bindingContext.reset();

    TextureHeapIndex index3 = bindingContext.bindTexture2D(view);
    CHECK(index3 < index1);
}