#include <catch2/catch_test_macros.hpp>

#include "TestUtils.hpp"

#include <Cory/Base/FmtUtils.hpp>
#include <Cory/Framegraph/Framegraph.hpp>
#include <Cory/Framegraph/RenderTaskBuilder.hpp>
#include <Cory/Framegraph/RenderTaskDeclaration.hpp>
#include <Cory/Renderer/FrameContext.hpp>
#include <Cory/Renderer/ShaderManager.hpp>

#include <KDGpu/texture_options.h>
#include <cppcoro/fmap.hpp>
#include <range/v3/algorithm/contains.hpp>

#include <gsl/gsl>

using namespace Cory;

namespace passes {

struct DepthPassOutputs {
    TransientTextureHandle depthTexture;
    TransientBufferHandle depthBuffer;
};

RenderTaskDeclaration<DepthPassOutputs>
depthPass(Context &ctx, RenderTaskBuilder builder, glm::u32vec3 size)
{
    auto depth = builder.create("TEX_depth",
                                size,
                                TextureFormat::D24_UNORM_S8_UINT,
                                Sync::AccessType::DepthStencilAttachmentWrite);
    auto depthBuffer = builder.create("BUF_depth",
                                      256u,
                                      Gpu::BufferUsageFlagBits::StorageBufferBit,
                                      Sync::AccessType::AnyShaderWrite);
    DepthPassOutputs outputs{depth, depthBuffer};

    static ShaderHandle vertexShader = ctx.shaders().createShader(
        R"slang(
struct VSInput {
    float3 position;
    float3 texCoord;
    float4 color;
};

struct VSOutput {
    float4 position : SV_Position;
};

VSOutput main(VSInput input) {
    VSOutput output;
    output.position = float4(input.position, 1.0);
    return output;
})slang",
        Gpu::ShaderStageFlagBits::VertexBit,
        "depth.vert");

    static ShaderHandle fragmentShader = ctx.shaders().createShader(
        R"slang(
struct VSOutput {
    float4 position : SV_Position;
};
struct PSOutput {
    float4 outColor : SV_Target0;
};
PSOutput main(VSOutput input) {
    PSOutput output;
    output.outColor = float4(1.0, 0.0, 0.0, 1.0);
    return output;
})slang",
        Gpu::ShaderStageFlagBits::FragmentBit,
        "depth.frag");

    auto depthPass = builder.declareRenderPass(RenderPassDeclaration{
        .name = "PASS_Depth",
        .shaders = {vertexShader, fragmentShader},
        .attachments = {},
        .depthAttachment =
            DepthStencilAttachment{
                depth,
                KDGpu::AttachmentLoadOperation::Clear,
                KDGpu::AttachmentStoreOperation::Store,
                1.0f,
            },
    });

    RenderInput render = co_await builder.finishDeclaration(outputs);
    CO_CORE_ASSERT(render.cmd != nullptr, "Uh-oh");
    auto recorder = depthPass.begin(*render.cmd);
    CO_APP_INFO("[DepthPrepass] render commands executing");
    recorder.end();
}

struct DepthDebugOut {
    TransientTextureHandle debugColor;
};
RenderTaskDeclaration<DepthDebugOut> depthDebug(Framegraph &graph,
                                                TransientTextureHandle depthInput)
{
    RenderTaskBuilder builder = graph.declareTask("TASK_DepthDebug");

    auto depthInfo = builder.read(
        depthInput, Sync::AccessType::FragmentShaderReadSampledImageOrUniformTexelBuffer);

    auto depthVis = builder.create("TEX_depthDebugVis",
                                   depthInfo.size,
                                   TextureFormat::R8G8B8A8_SRGB,
                                   Sync::AccessType::ColorAttachmentWrite);

    [[maybe_unused]] RenderInput render =
        co_await builder.finishDeclaration(DepthDebugOut{depthVis});

    CO_APP_INFO("[DepthDebug] Pass render commands are executed");
}

struct NormalDebugOut {
    TransientTextureHandle debugColor;
};
RenderTaskDeclaration<NormalDebugOut> normalDebug(Framegraph &graph,
                                                  TransientTextureHandle normalInput)
{
    RenderTaskBuilder builder = graph.declareTask("TASK_NormalDebug");

    auto normalInfo = builder.read(
        normalInput, Sync::AccessType::FragmentShaderReadSampledImageOrUniformTexelBuffer);

    auto normalVis = builder.create("TEX_normalDebugVis",
                                    normalInfo.size,
                                    TextureFormat::R8G8B8A8_SRGB,
                                    Sync::AccessType::ColorAttachmentWrite);

    [[maybe_unused]] RenderInput render =
        co_await builder.finishDeclaration(NormalDebugOut{normalVis});

    CO_APP_INFO("[NormalDebug] Pass render commands are executed");
}

struct DebugOut {
    TransientTextureHandle debugColor;
};
RenderTaskDeclaration<DebugOut> debugGeneral(Framegraph &graph,
                                             std::vector<TransientTextureHandle> debugTextures,
                                             gsl::index debugViewIndex)
{
    RenderTaskBuilder builder = graph.declareTask("TASK_GeneralDebug");

    auto &textureToDebug = debugTextures[debugViewIndex];
    auto dbgInfo = builder.read(
        textureToDebug, Sync::AccessType::FragmentShaderReadSampledImageOrUniformTexelBuffer);

    auto depthVis = builder.create("TEX_debugVis",
                                   dbgInfo.size,
                                   TextureFormat::R8G8B8A8_SRGB,
                                   Sync::AccessType::ColorAttachmentWrite);

    [[maybe_unused]] RenderInput render = co_await builder.finishDeclaration(DebugOut{depthVis});

    CO_APP_INFO("[Debug] Pass render commands are executed");
}

struct MainOut {
    TransientTextureHandle color;
    TransientTextureHandle normal;
};
RenderTaskDeclaration<MainOut> mainPass(RenderTaskBuilder builder,
                                        TransientTextureHandle colorInput,
                                        TransientTextureHandle normalInput,
                                        TransientTextureHandle depthInput,
                                        TransientBufferHandle bufferInput)
{
    auto depthInfo = builder.read(depthInput, Sync::AccessType::DepthStencilAttachmentRead);
    builder.readWrite(bufferInput, Sync::AccessType::AnyShaderWrite);

    auto colorOut =
        colorInput
            ? builder.readWrite(colorInput, Cory::Sync::AccessType::ColorAttachmentReadWrite).first
            : builder.create("TEX_color",
                             depthInfo.size,
                             TextureFormat::R8G8B8A8_SRGB,
                             Sync::AccessType::FragmentShaderReadSampledImageOrUniformTexelBuffer);
    auto normalOut = [&]() {
        if (normalInput) {
            return builder.readWrite(normalInput, Cory::Sync::AccessType::ColorAttachmentReadWrite)
                .first;
        }
        return builder.create("TEX_normal",
                              depthInfo.size,
                              TextureFormat::R8G8B8A8_UNORM,
                              Sync::AccessType::ColorAttachmentWrite);
    }();

    [[maybe_unused]] RenderInput render =
        co_await builder.finishDeclaration(MainOut{colorOut, normalOut});

    CO_APP_INFO("{} Pass render commands are executed", builder.name());
}

struct PostProcessOut {
    TransientTextureHandle color;
};
RenderTaskDeclaration<PostProcessOut> postProcess(RenderTaskBuilder builder,
                                                  TransientTextureHandle currentColorInput,
                                                  TransientTextureHandle previousColorInput)
{
    auto curColorInfo = builder.read(
        currentColorInput, Sync::AccessType::FragmentShaderReadSampledImageOrUniformTexelBuffer);
    auto prevColorInfo = builder.read(
        previousColorInput, Sync::AccessType::FragmentShaderReadSampledImageOrUniformTexelBuffer);

    auto color = builder.create("TEX_postprocess",
                                curColorInfo.size,
                                TextureFormat::R8G8B8A8_SRGB,
                                Sync::AccessType::ColorAttachmentWrite);

    [[maybe_unused]] RenderInput render = co_await builder.finishDeclaration(PostProcessOut{color});

    CO_APP_INFO("[Postprocess] Pass render commands are executed");
}

struct TempResourceOut {
    TransientTextureHandle color;
    TransientBufferHandle scratch;
};
RenderTaskDeclaration<TempResourceOut> tempResourcePass(RenderTaskBuilder builder,
                                                        glm::u32vec3 size)
{
    auto color = builder.create("TEX_tempColor",
                                size,
                                TextureFormat::R8G8B8A8_SRGB,
                                Sync::AccessType::ColorAttachmentWrite);
    auto scratch = builder.create("BUF_tempScratch",
                                  512u,
                                  Gpu::BufferUsageFlagBits::StorageBufferBit,
                                  Sync::AccessType::AnyShaderWrite);

    [[maybe_unused]] RenderInput render =
        co_await builder.finishDeclaration(TempResourceOut{color, scratch});
    CO_APP_INFO("[TempResource] Pass render commands are executed");
}
} // namespace passes

TEST_CASE("Framegraph API", "[Cory/Framegraph/Framegraph]")
{
    testing::VulkanTester t;

    Framegraph graph(t.ctx(), 0);

    auto &device = t.ctx().device();

    // Create a 2D sRGB color texture to use as a color attachment and for sampling
    Texture previousFrame = device.createTexture(Gpu::TextureOptions{
        .label = "TEX_previousFrameColor (IMG)",
        .type = Gpu::TextureType::TextureType2D,
        .format = TextureFormat::R8G8B8A8_SRGB,
        .extent = {1024, 768, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = Gpu::SampleCountFlagBits::Samples1Bit,
        .usage =
            Gpu::TextureUsageFlagBits::ColorAttachmentBit | Gpu::TextureUsageFlagBits::SampledBit,
        .memoryUsage = Gpu::MemoryUsage::GpuOnly,
    });

    // Create a default 2D view of the texture (keeps same format)
    TextureView previousFrameView = previousFrame.createView(Gpu::TextureViewOptions{
        .label = "TEX_previousFrameColor (VIEW)",
        .viewType = Gpu::ViewType::ViewType2D,
        .format = TextureFormat::R8G8B8A8_SRGB,
    });

    const TransientTextureHandle prevFrameColor = graph.declareInput(
        {"TEX_previousFrameColor", glm::u32vec3{1024, 768, 1}, TextureFormat::R8G8B8A8_SRGB},
        Sync::AccessType::None,
        previousFrame,
        previousFrameView);

    auto depthPass = passes::depthPass(t.ctx(), graph.declareTask("PASS_DepthPre"), {800, 600, 1});
    auto depthTex = depthPass.output().depthTexture;
    auto mainPass = passes::mainPass(graph.declareTask("PASS_Main"),
                                     NullHandle,
                                     NullHandle,
                                     depthTex,
                                     depthPass.output().depthBuffer);

    auto addMainPass = passes::mainPass(graph.declareTask("PASS_Main_Lines"),
                                        mainPass.output().color,
                                        mainPass.output().normal,
                                        depthTex,
                                        depthPass.output().depthBuffer);

    auto depthDebugPass = passes::depthDebug(graph, depthPass.output().depthTexture);
    auto normalDebugPass = passes::normalDebug(graph, mainPass.output().normal);
    auto debugCombinePass = passes::debugGeneral(
        graph, {depthDebugPass.output().debugColor, normalDebugPass.output().debugColor}, 0);

    auto postProcess = passes::postProcess(
        graph.declareTask("TASK_Postprocess"), addMainPass.output().color, prevFrameColor);

    // provoke the coroutines to run so we have some stuff to cull in the graph - otherwise
    // they won't even, that's how neat using coroutines is :)
    depthDebugPass.output();
    normalDebugPass.output();
    debugCombinePass.output();

    auto postprocessOut = postProcess.output();

    auto [resultInfo, resultState] = graph.declareOutput(postprocessOut.color);

    CO_APP_INFO("Final output is a color texture of {}", resultInfo.size);

    CommandRecorder recorder = device.createCommandRecorder(Gpu::CommandRecorderOptions{
        .label = "CMD_FramegraphTest",
        .queue = t.ctx().graphicsQueue().handle(),
        .level = Gpu::CommandBufferLevel::Primary,
    });

    FrameContext frameCtx{
        .inFlightIndex = 2,
        .swapchainImageIndex = 1,
        .frameNumber = 42,
        // rest not needed for the framegraph
        .commandBuffer = std::move(recorder),
    };
    auto g = graph.record(frameCtx);
    CHECK(!g.buffers.empty());
    CHECK(!g.bufferTransitions.empty());
    CO_APP_INFO(graph.dump(g));
}

TEST_CASE("Framegraph allocates temp resources for scheduled tasks",
          "[Cory/Framegraph/Framegraph]")
{
    testing::VulkanTester t;
    Framegraph graph(t.ctx(), 0);

    auto pass = passes::tempResourcePass(graph.declareTask("PASS_TempResource"), {128, 128, 1});
    auto [outputInfo, outputState] = graph.declareOutput(
        pass.output().color, Sync::AccessType::ColorAttachmentWrite);
    CHECK(outputInfo.size.x == 128);
    CHECK(outputState.status == TextureMemoryStatus::Virtual);

    CommandRecorder recorder = t.ctx().device().createCommandRecorder(Gpu::CommandRecorderOptions{
        .label = "CMD_FramegraphTempResourceTest",
        .queue = t.ctx().graphicsQueue().handle(),
        .level = Gpu::CommandBufferLevel::Primary,
    });

    FrameContext frameCtx{
        .inFlightIndex = 0,
        .swapchainImageIndex = 0,
        .frameNumber = 1,
        .commandBuffer = std::move(recorder),
    };

    auto execInfo = graph.record(frameCtx);
    const auto scratchHandle = FramegraphBufferHandle{pass.output().scratch};

    CHECK(ranges::contains(execInfo.buffers, scratchHandle));
    //CHECK(graph.resources().state(scratchHandle).status == BufferMemoryStatus::Allocated);
}
