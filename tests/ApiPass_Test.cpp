#include <Cory/Testing/TestUtils.hpp>

#include <catch2/catch_test_macros.hpp>

#include <Cory/Application/Window.hpp>
#include <Cory/Framegraph/Framegraph.hpp>
#include <Cory/Framegraph/FramegraphResourceManager.hpp>
#include <Cory/Framegraph/RenderTaskBuilder.hpp>
#include <Cory/Framegraph/RenderTaskDeclaration.hpp>
#include <Cory/Framegraph/ShaderBindingContext.hpp>
#include <Cory/Renderer/DescriptorSets.hpp>
#include <Cory/Renderer/FrameSource.hpp>
#include <Cory/Renderer/HeadlessFrameSource.hpp>

#include <KDGpu/device.h>
#include <KDGpu/sampler_options.h>
#include <KDGpu/texture_options.h>

#include <optional>
#include <type_traits>

namespace {

using namespace Cory;

struct PresetOutputs {
    TransientTextureHandle texture;
    TransientBufferHandle buffer;
};

RenderTaskDeclaration<PresetOutputs> presetTask(RenderTaskBuilder builder)
{
    auto texture = builder.create("TEX_preset",
                                  glm::u32vec3{8, 8, 1},
                                  Gpu::Format::R8G8B8A8_UNORM,
                                  Gpu::TextureUsageFlagBits::SampledBit |
                                      Gpu::TextureUsageFlagBits::StorageBit |
                                      Gpu::TextureUsageFlagBits::TransferDstBit,
                                  Sync::AccessType::None);

    auto buffer = builder.create("BUF_preset",
                                 256,
                                 Gpu::BufferUsageFlagBits::StorageBufferBit |
                                     Gpu::BufferUsageFlagBits::ShaderDeviceAddressBit,
                                 Sync::AccessType::None);

    (void)builder.read(texture, RenderTaskBuilder::TextureReadPreset::FragmentSampled);
    auto [writtenTexture, _writtenTextureInfo] =
        builder.write(texture, RenderTaskBuilder::TextureWritePreset::ComputeStorage);
    auto [rwTexture, _rwTextureInfo] = builder.readWrite(
        writtenTexture, RenderTaskBuilder::TextureReadWritePreset::GeneralStorage);

    (void)builder.read(buffer, RenderTaskBuilder::BufferReadPreset::VertexStorage);
    auto [writtenBuffer, _writtenBufferInfo] =
        builder.write(buffer, RenderTaskBuilder::BufferWritePreset::ComputeStorage);
    auto [rwBuffer, _rwBufferInfo] =
        builder.readWrite(writtenBuffer, RenderTaskBuilder::BufferReadWritePreset::ComputeStorage);

    [[maybe_unused]] RenderInput render =
        co_await builder.finishDeclaration(PresetOutputs{.texture = rwTexture, .buffer = rwBuffer});
}

const RenderTaskInfo::TextureDependency *findTextureDependency(const RenderTaskInfo &info,
                                                               TaskDependencyKind kind,
                                                               Sync::AccessType access,
                                                               Gpu::TextureUsageFlags usage)
{
    for (const auto &dep : info.textureDependencies) {
        if (dep.kind == kind && dep.access == access && dep.usage == usage) {
            return &dep;
        }
    }
    return nullptr;
}

const RenderTaskInfo::BufferDependency *findBufferDependency(const RenderTaskInfo &info,
                                                             TaskDependencyKind kind,
                                                             Sync::AccessType access,
                                                             Gpu::BufferUsageFlags usage)
{
    for (const auto &dep : info.bufferDependencies) {
        if (dep.kind == kind && dep.access == access && dep.usage == usage) {
            return &dep;
        }
    }
    return nullptr;
}

} // namespace

TEST_CASE("API pass: RenderTaskBuilder presets map to expected usage/access",
          "[Framegraph][RenderTaskBuilder]")
{
    using namespace Cory;

    testing::VulkanTester tester;
    FramegraphResourceManager resources(tester.ctx());
    Framegraph fg(tester.ctx(), resources, 0);

    auto declaration = presetTask(fg.declareTask("PresetTask"));
    [[maybe_unused]] auto outputs = declaration.output();

    std::optional<RenderTaskInfo> maybeTaskInfo;
    for (auto [handle, info] : fg.renderTasks()) {
        (void)handle;
        maybeTaskInfo = info;
        break;
    }

    REQUIRE(maybeTaskInfo.has_value());
    const auto &taskInfo = *maybeTaskInfo;

    REQUIRE(
        findTextureDependency(taskInfo,
                              TaskDependencyKindBits::Read,
                              Sync::AccessType::FragmentShaderReadSampledImageOrUniformTexelBuffer,
                              Gpu::TextureUsageFlagBits::SampledBit) != nullptr);
    REQUIRE(findTextureDependency(taskInfo,
                                  TaskDependencyKindBits::Write,
                                  Sync::AccessType::ComputeShaderWrite,
                                  Gpu::TextureUsageFlagBits::StorageBit) != nullptr);
    REQUIRE(findTextureDependency(taskInfo,
                                  TaskDependencyKindBits::ReadWrite,
                                  Sync::AccessType::General,
                                  Gpu::TextureUsageFlagBits::StorageBit) != nullptr);

    REQUIRE(findBufferDependency(taskInfo,
                                 TaskDependencyKindBits::Read,
                                 Sync::AccessType::VertexShaderReadOther,
                                 Gpu::BufferUsageFlagBits::StorageBufferBit) != nullptr);
    REQUIRE(findBufferDependency(taskInfo,
                                 TaskDependencyKindBits::Write,
                                 Sync::AccessType::ComputeShaderWrite,
                                 Gpu::BufferUsageFlagBits::StorageBufferBit |
                                     Gpu::BufferUsageFlagBits::ShaderDeviceAddressBit) != nullptr);
    REQUIRE(findBufferDependency(taskInfo,
                                 TaskDependencyKindBits::ReadWrite,
                                 Sync::AccessType::General,
                                 Gpu::BufferUsageFlagBits::StorageBufferBit |
                                     Gpu::BufferUsageFlagBits::ShaderDeviceAddressBit) != nullptr);
}

TEST_CASE("API pass: ScopedBinding unbinds context on scope exit", "[ShaderBindingContext]")
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

    auto colorTexture = tester.ctx().device().createTexture(Gpu::TextureOptions{
        .label = "ScopedBindingColor",
        .type = Gpu::TextureType::TextureType2D,
        .format = Gpu::Format::B8G8R8A8_UNORM,
        .extent = {4, 4, 1},
        .mipLevels = 1,
        .usage = Gpu::TextureUsageFlagBits::ColorAttachmentBit,
        .memoryUsage = Gpu::MemoryUsage::GpuOnly,
    });
    auto colorView = colorTexture.createView(Gpu::TextureViewOptions{
        .label = "ScopedBindingColorView",
    });

    auto pipelineLayout = tester.ctx().device().createPipelineLayout(Gpu::PipelineLayoutOptions{
        .label = "ScopedBindingLayout",
        .bindGroupLayouts = descriptorSets.layouts(),
        .pushConstantRanges = {},
    });

    auto cmd = tester.ctx().device().createCommandRecorder(
        Gpu::CommandRecorderOptions{.label = "ScopedBindingLifecycleTest"});
    auto render = cmd.beginRenderPass(Gpu::RenderPassCommandRecorderOptions{
        .colorAttachments = {{
            .view = colorView,
            .finalLayout = Gpu::TextureLayout::ColorAttachmentOptimal,
        }},
    });

    {
        auto scope = bindingContext.scoped(render, pipelineLayout, true);
        (void)bindingContext.bindSampler(sampler.handle());
        // No explicit flush/unbind here: scope teardown must handle both.
    }

    // If first scope failed to unbind, this bind would assert because a recorder is already active.
    {
        auto scope = bindingContext.scoped(render, pipelineLayout, false);
        scope.release();
    }

    render.end();
    [[maybe_unused]] auto commandBuffer = cmd.finish();
}

TEST_CASE("API pass: FrameSource contract works for headless source", "[FrameSource]")
{
    using namespace Cory;

    testing::VulkanTester tester;

    auto headless = HeadlessFrameSource(tester.ctx(),
                                        HeadlessFrameSourceCreateInfo{
                                            .label = "FrameSourceContractTest",
                                            .size = {320, 240},
                                            .samples = Gpu::SampleCountFlagBits::Samples1Bit,
                                            .colorFormat = Gpu::Format::B8G8R8A8_UNORM,
                                            .imageCount = 2,
                                        });

    static_assert(std::is_base_of_v<FrameSource, Window>);
    static_assert(std::is_base_of_v<FrameSource, HeadlessFrameSource>);

    FrameSource &frameSource = headless;
    CHECK(frameSource.extent() == glm::u32vec2{320, 240});
    CHECK(frameSource.sampleCount() == Gpu::SampleCountFlagBits::Samples1Bit);
    CHECK(frameSource.colorFormat() == Gpu::Format::B8G8R8A8_UNORM);
    CHECK(frameSource.size() == 2);

    // Do not force iteration in this test; this validates the shared metadata contract only.
    [[maybe_unused]] auto frames = frameSource.frames();
}
