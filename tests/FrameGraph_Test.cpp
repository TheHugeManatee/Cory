#include <catch2/catch_test_macros.hpp>

#include "TestUtils.hpp"

#include <Cory/Base/FmtUtils.hpp>
#include <Cory/Framegraph/Framegraph.hpp>
#include <Cory/Framegraph/RenderTaskDeclaration.hpp>
#include <Cory/Renderer/FrameContext.hpp>
#include <Cory/Renderer/ShaderManager.hpp>
#include <KDGpu/texture_options.h>

#include <cppcoro/fmap.hpp>

#include <gsl/gsl>

using namespace Cory;

namespace passes {

struct DepthPassOutputs {
    TransientTextureHandle depthTexture;
};

RenderTaskDeclaration<DepthPassOutputs>
depthPass(Context &ctx, RenderTaskBuilder builder, glm::u32vec3 size)
{
    auto depth = builder.create("TEX_depth",
                                size,
                                TextureFormat::D32_SFLOAT,
                                Sync::AccessType::DepthStencilAttachmentWrite);
    DepthPassOutputs outputs{depth};

    static ShaderHandle vertexShader = ctx.shaders().createShader(
        R"glsl(#version 450
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inTexCoord;
layout(location = 2) in vec4 inColor;
void main() {
    gl_Position = vec4(inPosition, 1.0);
}
)glsl",
        Gpu::ShaderStageFlagBits::VertexBit,
        "depth.vert");

    static ShaderHandle fragmentShader = ctx.shaders().createShader(
        R"glsl(#version 450
layout(location = 0) out vec4 outColor;
void main() {
    outColor = gl_FragCoord;
}
)glsl",
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

    co_yield outputs;
    RenderInput render = co_await builder.finishDeclaration();
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

    co_yield DepthDebugOut{depthVis};
    RenderInput render = co_await builder.finishDeclaration();

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

    co_yield NormalDebugOut{normalVis};
    RenderInput render = co_await builder.finishDeclaration();

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

    co_yield DebugOut{depthVis};
    RenderInput render = co_await builder.finishDeclaration();

    CO_APP_INFO("[Debug] Pass render commands are executed");
}

struct MainOut {
    TransientTextureHandle color;
    TransientTextureHandle normal;
};
RenderTaskDeclaration<MainOut> mainPass(RenderTaskBuilder builder,
                                        TransientTextureHandle colorInput,
                                        TransientTextureHandle normalInput,
                                        TransientTextureHandle depthInput)
{
    auto depthInfo = builder.read(depthInput, Sync::AccessType::DepthStencilAttachmentRead);

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

    co_yield MainOut{colorOut, normalOut};
    RenderInput render = co_await builder.finishDeclaration();

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

    co_yield PostProcessOut{color};
    RenderInput render = co_await builder.finishDeclaration();

    CO_APP_INFO("[Postprocess] Pass render commands are executed");
}
} // namespace passes

TEST_CASE("Framegraph API", "[Cory/Framegraph/Framegraph]")
{
    testing::VulkanTester t;

    Framegraph graph(t.ctx());

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
    auto mainPass =
        passes::mainPass(graph.declareTask("PASS_Main"), NullHandle, NullHandle, depthTex);

    auto addMainPass = passes::mainPass(graph.declareTask("PASS_Main_Lines"),
                                        mainPass.output().color,
                                        mainPass.output().normal,
                                        depthTex);

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
    CO_APP_INFO(graph.dump(g));
}