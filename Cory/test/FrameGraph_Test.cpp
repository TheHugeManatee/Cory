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
namespace FG = Cory::Framegraph;
namespace Vk = Magnum::Vk;

namespace passes {

struct DepthPassOutputs {
    FG::TransientTextureHandle depthTexture;
};
FG::RenderTaskDeclaration<DepthPassOutputs>
depthPass(Context &ctx, FG::Builder builder, glm::u32vec3 size)
{
    auto depth = builder.create(
        "depthTexture", size, PixelFormat::Depth32F, Sync::AccessType::DepthStencilAttachmentWrite);
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
    FG::RenderInput render = co_await builder.finishDeclaration();
    CO_CORE_ASSERT(render.cmd != nullptr, "Uh-oh");
    depthPass.begin(*render.cmd);
    CO_APP_INFO("[DepthPrepass] render commands executing");
    depthPass.end(*render.cmd);
}

struct DepthDebugOut {
    FG::TransientTextureHandle debugColor;
};
FG::RenderTaskDeclaration<DepthDebugOut> depthDebug(FG::Framegraph &graph,
                                                    FG::TransientTextureHandle depthInput)
{
    FG::Builder builder = graph.declareTask("DepthDebug");

    auto depthInfo = builder.read(
        depthInput, Sync::AccessType::FragmentShaderReadSampledImageOrUniformTexelBuffer);

    auto depthVis = builder.create("depthDebugVis",
                                   depthInfo.size,
                                   PixelFormat::RGBA8Srgb,
                                   Sync::AccessType::ColorAttachmentWrite);

    co_yield DepthDebugOut{depthVis};
    FG::RenderInput render = co_await builder.finishDeclaration();

    CO_APP_INFO("[DepthDebug] Pass render commands are executed");
}

struct NormalDebugOut {
    FG::TransientTextureHandle debugColor;
};
FG::RenderTaskDeclaration<NormalDebugOut> normalDebug(FG::Framegraph &graph,
                                                      FG::TransientTextureHandle normalInput)
{
    FG::Builder builder = graph.declareTask("NormalDebug");

    auto normalInfo = builder.read(
        normalInput, Sync::AccessType::FragmentShaderReadSampledImageOrUniformTexelBuffer);

    auto normalVis = builder.create("normalDebugVis",
                                    normalInfo.size,
                                    PixelFormat::RGBA8Srgb,
                                    Sync::AccessType::ColorAttachmentWrite);

    co_yield NormalDebugOut{normalVis};
    FG::RenderInput render = co_await builder.finishDeclaration();

    CO_APP_INFO("[NormalDebug] Pass render commands are executed");
}

struct DebugOut {
    FG::TransientTextureHandle debugColor;
};
FG::RenderTaskDeclaration<DebugOut>
debugGeneral(FG::Framegraph &graph,
             std::vector<FG::TransientTextureHandle> debugTextures,
             gsl::index debugViewIndex)
{
    FG::Builder builder = graph.declareTask("GeneralDebug");

    auto &textureToDebug = debugTextures[debugViewIndex];
    auto dbgInfo = builder.read(
        textureToDebug, Sync::AccessType::FragmentShaderReadSampledImageOrUniformTexelBuffer);

    auto depthVis = builder.create(
        "debugVis", dbgInfo.size, PixelFormat::RGBA8Srgb, Sync::AccessType::ColorAttachmentWrite);

    co_yield DebugOut{depthVis};
    FG::RenderInput render = co_await builder.finishDeclaration();

    CO_APP_INFO("[Debug] Pass render commands are executed");
}

struct MainOut {
    FG::TransientTextureHandle color;
    FG::TransientTextureHandle normal;
};
FG::RenderTaskDeclaration<MainOut> mainPass(FG::Framegraph &graph,
                                            FG::TransientTextureHandle depthInput)
{
    FG::Builder builder = graph.declareTask("MainPass");

    auto depthInfo = builder.read(
        depthInput, Sync::AccessType::FragmentShaderReadSampledImageOrUniformTexelBuffer);

    auto color = builder.create("colorTexture",
                                depthInfo.size,
                                PixelFormat::RGBA8Srgb,
                                Sync::AccessType::ColorAttachmentWrite);
    auto normal = builder.create("normalTexture",
                                 depthInfo.size,
                                 PixelFormat::RGBA8Unorm,
                                 Sync::AccessType::ColorAttachmentWrite);

    co_yield MainOut{color, normal};
    FG::RenderInput render = co_await builder.finishDeclaration();

    // render portion
    //    struct PassTextures {
    //        FG::Texture color;
    //        FG::Texture depth;
    //        FG::Texture normal;
    //    };

    CO_APP_INFO("[MainPass] Pass render commands are executed");
}

struct PostProcessOut {
    FG::TransientTextureHandle color;
};
FG::RenderTaskDeclaration<PostProcessOut> postProcess(FG::Framegraph &graph,
                                                      FG::TransientTextureHandle currentColorInput,
                                                      FG::TransientTextureHandle previousColorInput)
{
    FG::Builder builder = graph.declareTask("Postprocess");

    auto curColorInfo = builder.read(
        currentColorInput, Sync::AccessType::FragmentShaderReadSampledImageOrUniformTexelBuffer);
    auto prevColorInfo = builder.read(
        previousColorInput, Sync::AccessType::FragmentShaderReadSampledImageOrUniformTexelBuffer);

    auto color = builder.create("postprocessTexture",
                                curColorInfo.size,
                                PixelFormat::RGBA8Srgb,
                                Sync::AccessType::ColorAttachmentWrite);

    co_yield PostProcessOut{color};
    FG::RenderInput render = co_await builder.finishDeclaration();

    CO_APP_INFO("[Postprocess] Pass render commands are executed");
}
} // namespace passes

TEST_CASE("Framegraph API", "[Cory/Framegraph/Framegraph]")
{
    testing::VulkanTester t;

    namespace Vk = Magnum::Vk;

    FG::Framegraph graph(t.ctx());
    Vk::Image prevFrame{t.ctx().device(),
                        Vk::ImageCreateInfo2D{Vk::ImageUsage::ColorAttachment,
                                              Vk::PixelFormat::RGBA8Srgb,
                                              Magnum::Vector2i{1024, 768},
                                              1},
                        Vk::MemoryFlag::DeviceLocal};
    Vk::ImageView prevFrameView{t.ctx().device(), Vk::ImageViewCreateInfo2D{prevFrame}};

    const FG::TransientTextureHandle prevFrameColor = graph.declareInput(
        {"previousFrameColor", glm::u32vec3{1024, 768, 1}, PixelFormat::RGBA8Srgb},
        Sync::AccessType::None,
        prevFrame,
        prevFrameView);

    auto depthPass = passes::depthPass(t.ctx(), graph.declareTask("depthPrepass"), {800, 600, 1});
    auto mainPass = passes::mainPass(graph, depthPass.output().depthTexture);

    auto depthDebugPass = passes::depthDebug(graph, depthPass.output().depthTexture);
    auto normalDebugPass = passes::normalDebug(graph, mainPass.output().normal);
    auto debugCombinePass = passes::debugGeneral(
        graph, {depthDebugPass.output().debugColor, normalDebugPass.output().debugColor}, 0);

    auto postProcess = passes::postProcess(graph, mainPass.output().color, prevFrameColor);

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
    nameVulkanObject(t.ctx().device(), buffer, "Framegraph Test");

    buffer.begin();

    auto g = graph.record(buffer);
    CO_APP_INFO(graph.dump(g));

    buffer.end();
}