#include <catch2/catch_test_macros.hpp>

#include "TestUtils.hpp"

#include <Cory/Base/FmtUtils.hpp>
#include <Cory/Framegraph/CommandList.hpp>
#include <Cory/Framegraph/Framegraph.hpp>
#include <Cory/Framegraph/RenderPassDeclaration.hpp>
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
    FG::TextureHandle depthTexture;
};
FG::RenderPassDeclaration<DepthPassOutputs>
depthPass(Context &ctx, FG::Builder builder, glm::u32vec3 size)
{
    auto depth = builder.create("depthTexture",
                                size,
                                PixelFormat::Depth32F,
                                FG::Layout::Attachment,
                                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
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
    FG::TextureHandle debugColor;
};
FG::RenderPassDeclaration<DepthDebugOut> depthDebug(FG::Framegraph &graph,
                                                    FG::TextureHandle depthInput)
{
    FG::Builder builder = graph.declareTask("DepthDebug");

    auto depthInfo = builder.read(depthInput,
                                  {.layout = FG::Layout::Attachment,
                                   .stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                   .access = VK_ACCESS_2_NONE,
                                   .imageAspect = VK_IMAGE_ASPECT_DEPTH_BIT});

    auto depthVis = builder.create("depthDebugVis",
                                   depthInfo.size,
                                   PixelFormat::RGBA8Srgb,
                                   FG::Layout::Attachment,
                                   VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                   VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

    co_yield DepthDebugOut{depthVis};
    FG::RenderInput render = co_await builder.finishDeclaration();

    CO_APP_INFO("[DepthDebug] Pass render commands are executed");
}

struct NormalDebugOut {
    FG::TextureHandle debugColor;
};
FG::RenderPassDeclaration<NormalDebugOut> normalDebug(FG::Framegraph &graph,
                                                      FG::TextureHandle normalInput)
{
    FG::Builder builder = graph.declareTask("NormalDebug");

    auto normalInfo = builder.read(normalInput,
                                   {.layout = FG::Layout::Attachment,
                                    .stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                    .access = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
                                    .imageAspect = VK_IMAGE_ASPECT_COLOR_BIT});

    auto normalVis = builder.create("normalDebugVis",
                                    normalInfo.size,
                                    PixelFormat::RGBA8Srgb,
                                    FG::Layout::Attachment,
                                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                    VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

    co_yield NormalDebugOut{normalVis};
    FG::RenderInput render = co_await builder.finishDeclaration();

    CO_APP_INFO("[NormalDebug] Pass render commands are executed");
}

struct DebugOut {
    FG::TextureHandle debugColor;
};
FG::RenderPassDeclaration<DebugOut> debugGeneral(FG::Framegraph &graph,
                                                 std::vector<FG::TextureHandle> debugTextures,
                                                 gsl::index debugViewIndex)
{
    FG::Builder builder = graph.declareTask("GeneralDebug");

    auto &textureToDebug = debugTextures[debugViewIndex];
    auto dbgInfo = builder.read(textureToDebug,
                                {.layout = FG::Layout::Attachment,
                                 .stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                 .access = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
                                 .imageAspect = VK_IMAGE_ASPECT_DEPTH_BIT});

    auto depthVis = builder.create("debugVis",
                                   dbgInfo.size,
                                   PixelFormat::RGBA8Srgb,
                                   FG::Layout::Attachment,
                                   VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                   VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

    co_yield DebugOut{depthVis};
    FG::RenderInput render = co_await builder.finishDeclaration();

    CO_APP_INFO("[Debug] Pass render commands are executed");
}

struct MainOut {
    FG::TextureHandle color;
    FG::TextureHandle normal;
};
FG::RenderPassDeclaration<MainOut> mainPass(FG::Framegraph &graph, FG::TextureHandle depthInput)
{
    FG::Builder builder = graph.declareTask("MainPass");

    auto depthInfo = builder.read(depthInput,
                                  {.layout = FG::Layout::Attachment,
                                   .stage = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                                   .access = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
                                   .imageAspect = VK_IMAGE_ASPECT_DEPTH_BIT});

    auto color = builder.create("colorTexture",
                                depthInfo.size,
                                PixelFormat::RGBA8Srgb,
                                FG::Layout::Attachment,
                                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT);
    auto normal = builder.create("normalTexture",
                                 depthInfo.size,
                                 PixelFormat::RGBA8Unorm,
                                 FG::Layout::Attachment,
                                 VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                 VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT);

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
    FG::TextureHandle color;
};
FG::RenderPassDeclaration<PostProcessOut> postProcess(FG::Framegraph &graph,
                                                      FG::TextureHandle currentColorInput,
                                                      FG::TextureHandle previousColorInput)
{
    FG::Builder builder = graph.declareTask("Postprocess");

    auto curColorInfo = builder.read(currentColorInput,
                                     {.layout = FG::Layout::Attachment,
                                      .stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                      .access = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
                                      .imageAspect = VK_IMAGE_ASPECT_COLOR_BIT});
    auto prevColorInfo = builder.read(previousColorInput,
                                      {.layout = FG::Layout::Attachment,
                                       .stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                       .access = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
                                       .imageAspect = VK_IMAGE_ASPECT_COLOR_BIT});

    auto color = builder.create("postprocessTexture",
                                curColorInfo.size,
                                PixelFormat::RGBA8Srgb,
                                FG::Layout::Attachment,
                                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT);

    co_yield PostProcessOut{color};
    FG::RenderInput render = co_await builder.finishDeclaration();

    CO_APP_INFO("[Postprocess] Pass render commands are executed");
}
} // namespace passes

TEST_CASE("Framegraph API exploration", "[Cory/Framegraph/Framegraph]")
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

    const FG::TextureHandle prevFrameColor = graph.declareInput(
        {"previousFrameColor", glm::u32vec3{1024, 768, 1}, PixelFormat::RGBA8Srgb},
        FG::Layout::Attachment,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
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

    buffer.begin();
    graph.execute(buffer);
    buffer.end();
}