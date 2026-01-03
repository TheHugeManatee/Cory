#include "TestUtils.hpp"

#include <catch2/catch_test_macros.hpp>

#include <Cory/Framegraph/FramegraphResourceManager.hpp>
#include <Cory/Renderer/DescriptorSets.hpp>
#include <Cory/Framegraph/ShaderBindingContext.hpp>

#include <KDGpu/buffer_options.h>
#include <KDGpu/sampler_options.h>
#include <KDGpu/texture_options.h>

TEST_CASE("Shader binding context: Memory bump allocation", "[ShaderBindingContext]")
{
    using namespace Cory;

    testing::VulkanTester tester;

    DescriptorSets descriptorSets;

    descriptorSets.init(tester.ctx().device(), DescriptorSetOptions{.label = "Test Bindings"});
    FramegraphResourceManager resources(tester.ctx());
    ShaderBindingContext bindingContext(
        tester.ctx().device(), resources, descriptorSets, 0, 1024 * 1024);

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

    descriptorSets.init(tester.ctx().device(), DescriptorSetOptions{.label = "Test Bindings"});
    FramegraphResourceManager resources(tester.ctx());
    ShaderBindingContext bindingContext(
        tester.ctx().device(), resources, descriptorSets, 0, 1024 * 1024);

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
    auto sampler = tester.ctx().device().createSampler(Gpu::SamplerOptions{
        .magFilter = Gpu::FilterMode::Linear,
        .minFilter = Gpu::FilterMode::Linear,
    });

    TextureHeapIndex index1 =
        bindingContext.bindTexture2D(view, Gpu::TextureLayout::ShaderReadOnlyOptimal, sampler);
    TextureHeapIndex index2 =
        bindingContext.bindTexture2D(view, Gpu::TextureLayout::ShaderReadOnlyOptimal, sampler);

    CHECK(index2 > index1);

    bindingContext.reset();

    TextureHeapIndex index3 =
        bindingContext.bindTexture2D(view, Gpu::TextureLayout::ShaderReadOnlyOptimal, sampler);
    CHECK(index3 < index1);
}

TEST_CASE("Shader binding context: Texture 3D and storage image allocation",
          "[ShaderBindingContext]")
{
    using namespace Cory;

    testing::VulkanTester tester;

    DescriptorSets descriptorSets;

    descriptorSets.init(tester.ctx().device(), DescriptorSetOptions{.label = "Test Bindings"});
    FramegraphResourceManager resources(tester.ctx());
    ShaderBindingContext bindingContext(
        tester.ctx().device(), resources, descriptorSets, 0, 1024 * 1024);

    auto tex3d = tester.ctx().device().createTexture(Gpu::TextureOptions{
        .label = "Test Texture 3D",
        .type = Gpu::TextureType::TextureType3D,
        .format = Gpu::Format::R8G8B8A8_UNORM,
        .extent{4, 4, 4},
        .mipLevels = 1,
        .arrayLayers = 1,
        .usage = Gpu::TextureUsageFlagBits::SampledBit | Gpu::TextureUsageFlagBits::StorageBit,
        .memoryUsage = Gpu::MemoryUsage::GpuOnly,
    });
    auto view3d = tex3d.createView(Gpu::TextureViewOptions{
        .label = "Test Texture 3D View",
    });
    auto sampler = tester.ctx().device().createSampler(Gpu::SamplerOptions{
        .magFilter = Gpu::FilterMode::Linear,
        .minFilter = Gpu::FilterMode::Linear,
    });

    const TextureHeapIndex t1 =
        bindingContext.bindTexture3D(view3d, Gpu::TextureLayout::ShaderReadOnlyOptimal, sampler);
    const TextureHeapIndex t2 =
        bindingContext.bindTexture3D(view3d, Gpu::TextureLayout::ShaderReadOnlyOptimal, sampler);
    CHECK(t2 > t1);

    const TextureHeapIndex s1 =
        bindingContext.bindStorageImage3D(view3d, Gpu::TextureLayout::General);
    const TextureHeapIndex s2 =
        bindingContext.bindStorageImage3D(view3d, Gpu::TextureLayout::General);
    CHECK(s2 > s1);

    bindingContext.reset();

    auto tex2d = tester.ctx().device().createTexture(Gpu::TextureOptions{
        .label = "Test Texture 2D Storage",
        .type = Gpu::TextureType::TextureType2D,
        .format = Gpu::Format::R8G8B8A8_UNORM,
        .extent{4, 4, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .usage = Gpu::TextureUsageFlagBits::StorageBit,
        .memoryUsage = Gpu::MemoryUsage::GpuOnly,
    });
    auto view2d = tex2d.createView(Gpu::TextureViewOptions{
        .label = "Test Texture 2D View",
    });

    const TextureHeapIndex s3 =
        bindingContext.bindStorageImage2D(view2d, Gpu::TextureLayout::General);
    CHECK(s3 == 0);
}

TEST_CASE("Shader binding context: Sampler and buffer allocation", "[ShaderBindingContext]")
{
    using namespace Cory;

    testing::VulkanTester tester;

    DescriptorSets descriptorSets;

    descriptorSets.init(tester.ctx().device(), DescriptorSetOptions{.label = "Test Bindings"});
    FramegraphResourceManager resources(tester.ctx());
    ShaderBindingContext bindingContext(
        tester.ctx().device(), resources, descriptorSets, 0, 1024 * 1024);

    auto sampler = tester.ctx().device().createSampler(Gpu::SamplerOptions{
        .magFilter = Gpu::FilterMode::Nearest,
        .minFilter = Gpu::FilterMode::Nearest,
    });

    const SamplerHeapIndex s1 = bindingContext.bindSampler(sampler.handle());
    const SamplerHeapIndex s2 = bindingContext.bindSampler(sampler.handle());
    CHECK(s2 > s1);

    auto buffer = tester.ctx().device().createBuffer(Gpu::BufferOptions{
        .label = "Test Buffer",
        .size = 256,
        .usage = Gpu::BufferUsageFlagBits::StorageBufferBit,
        .memoryUsage = Gpu::MemoryUsage::CpuToGpu,
    });

    const BufferHeapIndex b1 =
        bindingContext.bindBuffer(buffer.handle(), BufferBindPoint::StorageBufferReadOnly);
    const BufferHeapIndex b2 =
        bindingContext.bindBuffer(buffer.handle(), BufferBindPoint::StorageBufferReadOnly);
    const BufferHeapIndex b3 =
        bindingContext.bindBuffer(buffer.handle(), BufferBindPoint::StorageBufferReadWrite);
    const BufferHeapIndex b4 =
        bindingContext.bindBuffer(buffer.handle(), BufferBindPoint::StorageBufferReadWrite);
    CHECK(b2 > b1);
    CHECK(b4 > b3);

    bindingContext.reset();

    const BufferHeapIndex b5 =
        bindingContext.bindBuffer(buffer.handle(), BufferBindPoint::StorageBufferReadOnly);
    CHECK(b5 == 0);
}
