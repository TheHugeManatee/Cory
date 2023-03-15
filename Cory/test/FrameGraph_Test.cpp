#include <catch2/catch_test_macros.hpp>

#include "TestUtils.hpp"

#include <Cory/Base/FmtUtils.hpp>
#include <Cory/Framegraph/CommandList.hpp>
#include <Cory/Framegraph/Framegraph.hpp>
#include <Cory/Framegraph/RenderTaskDeclaration.hpp>
#include <Cory/Renderer/ResourceManager.hpp>

#include <Magnum/Vk/CommandBuffer.h>
#include <Magnum/Vk/CommandPool.h>
#include <Magnum/Vk/Image.h>
#include <Magnum/Vk/ImageCreateInfo.h>
#include <Magnum/Vk/ImageViewCreateInfo.h>

#include <cppcoro/fmap.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/when_all.hpp>

#include <gsl/gsl>

using namespace Cory;

namespace Vk = Magnum::Vk;

namespace passes {

struct DepthPassOutputs {
    TransientTextureHandle depthTexture;
};
RenderTaskDeclaration<DepthPassOutputs>
depthPass(Context &ctx, RenderTaskBuilder builder, glm::u32vec3 size)
{
    auto depth = builder.create(
        "TEX_depth", size, PixelFormat::Depth32F, Sync::AccessType::DepthStencilAttachmentWrite);
    DepthPassOutputs outputs{depth};

    static ShaderHandle vertexShader = ctx.resources().createShader(
        R"glsl(#version 450
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inTexCoord;
layout(location = 2) in vec4 inColor;
void main() {
    gl_Position = vec4(inPosition, 1.0);
}
)glsl",
        ShaderType::eVertex,
        "depth.vert");

    static ShaderHandle fragmentShader = ctx.resources().createShader(
        R"glsl(#version 450
layout(location = 0) out vec4 outColor;
void main() {
    outColor = gl_FragCoord;
}
)glsl",
        ShaderType::eFragment,
        "depth.frag");

    auto depthPass =
        builder.declareRenderPass()
            .attachDepth(depth, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, 1.0f)
            .shaders({vertexShader, fragmentShader})
            .finish();

    co_yield outputs;
    RenderInput render = co_await builder.finishDeclaration();
    CO_CORE_ASSERT(render.cmd != nullptr, "Uh-oh");
    depthPass.begin(*render.cmd);
    CO_APP_INFO("[DepthPrepass] render commands executing");
    depthPass.end(*render.cmd);
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
                                   PixelFormat::RGBA8Srgb,
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
                                    PixelFormat::RGBA8Srgb,
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
                                   PixelFormat::RGBA8Srgb,
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
                             PixelFormat::RGBA8Srgb,
                             Sync::AccessType::FragmentShaderReadSampledImageOrUniformTexelBuffer);
    auto normalOut = [&]() {
        if (normalInput) {
            return builder.readWrite(normalInput, Cory::Sync::AccessType::ColorAttachmentReadWrite)
                .first;
        }
        return builder.create("TEX_normal",
                              depthInfo.size,
                              PixelFormat::RGBA8Unorm,
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
                                PixelFormat::RGBA8Srgb,
                                Sync::AccessType::ColorAttachmentWrite);

    co_yield PostProcessOut{color};
    RenderInput render = co_await builder.finishDeclaration();

    CO_APP_INFO("[Postprocess] Pass render commands are executed");
}
} // namespace passes

TEST_CASE("Framegraph API", "[Cory/Framegraph/Framegraph]")
{
    testing::VulkanTester t;

    namespace Vk = Magnum::Vk;

    Framegraph graph(t.ctx());
    Vk::Image prevFrame{
        t.ctx().device(),
        Vk::ImageCreateInfo2D{Vk::ImageUsage::ColorAttachment | Vk::ImageUsage::Sampled,
                              Vk::PixelFormat::RGBA8Srgb,
                              Magnum::Vector2i{1024, 768},
                              1},
        Vk::MemoryFlag::DeviceLocal};
    nameVulkanObject(t.ctx().device(), prevFrame, "TEX_previousFrameColor (IMG)");
    Vk::ImageView prevFrameView{t.ctx().device(), Vk::ImageViewCreateInfo2D{prevFrame}};
    nameVulkanObject(t.ctx().device(), prevFrameView, "TEX_previousFrameColor (VIEW)");

    const TransientTextureHandle prevFrameColor = graph.declareInput(
        {"TEX_previousFrameColor", glm::u32vec3{1024, 768, 1}, PixelFormat::RGBA8Srgb},
        Sync::AccessType::None,
        prevFrame,
        prevFrameView);

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

    CO_APP_INFO("Final output is a color texture of {}x{}x{}",
                resultInfo.size.x,
                resultInfo.size.y,
                resultInfo.size.z);

    Magnum::Vk::CommandBuffer buffer = t.ctx().commandPool().allocate();
    nameVulkanObject(t.ctx().device(), buffer, "CMD_FramegraphTest");

    buffer.begin();

    FrameContext frameCtx{
        .index = 2,
        .frameNumber = 42,
        // rest not needed for the framegraph
        .commandBuffer = &buffer,
    };
    auto g = graph.record(frameCtx);
    CO_APP_INFO(graph.dump(g));

    buffer.end();
}