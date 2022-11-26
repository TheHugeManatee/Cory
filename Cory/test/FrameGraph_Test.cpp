#include <catch2/catch_test_macros.hpp>

#include "TestUtils.hpp"

#include <Cory/Base/FmtUtils.hpp>
#include <Cory/Framegraph/CommandList.hpp>
#include <Cory/Framegraph/Framegraph.hpp>
#include <Cory/Framegraph/RenderPassDeclaration.hpp>

#include <Magnum/Vk/CommandBuffer.h>
#include <Magnum/Vk/CommandPool.h>
#include <Magnum/Vk/Image.h>

#include <cppcoro/fmap.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/when_all.hpp>

#include <gsl/gsl>

using namespace Cory;
namespace FG = Cory::Framegraph;

namespace passes {

struct DepthPassOutputs {
    FG::TextureHandle depthTexture;
};
FG::RenderPassDeclaration<DepthPassOutputs> depthPass(FG::Framegraph &graph, glm::u32vec3 size)
{
    PipelineHandle depthPipeline = {/*TODO*/};

    FG::Builder builder = graph.declarePass("DepthPrepass");

    auto depth = builder.create("depthTexture",
                                size,
                                FG::PixelFormat::D32,
                                FG::Layout::Attachment,
                                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                VK_ACCESS_2_SHADER_WRITE_BIT);
    DepthPassOutputs outputs{depth};

    co_yield outputs;
    FG::RenderInput render = co_await builder.finishDeclaration();

    // render.cmd->bind(depthPipeline);

    //    render.cmd->setupRenderPass()
    //        .attachDepth(outputs.depthTexture,
    //                     VK_ATTACHMENT_LOAD_OP_CLEAR,
    //                     VK_ATTACHMENT_STORE_OP_STORE,
    //                     {.depth = 1.0f, .stencil = 0})
    //        .begin();

    CO_APP_INFO("[DepthPrepass] render commands executing");

    //    render.cmd->endPass();
}

struct DepthDebugOut {
    FG::TextureHandle debugColor;
};
FG::RenderPassDeclaration<DepthDebugOut> depthDebug(FG::Framegraph &graph,
                                                    FG::TextureHandle depthInput)
{
    FG::Builder builder = graph.declarePass("DepthDebug");

    auto depthInfo = builder.read(depthInput,
                                  {.layout = FG::Layout::Attachment,
                                   .access = VK_ACCESS_2_SHADER_READ_BIT,
                                   .stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                   .imageAspect = VK_IMAGE_ASPECT_DEPTH_BIT});

    auto depthVis = builder.create("depthDebugVis",
                                   depthInfo.size,
                                   FG::PixelFormat::RGBA32,
                                   FG::Layout::Attachment,
                                   VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                   VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);

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
    FG::Builder builder = graph.declarePass("NormalDebug");

    auto normalInfo = builder.read(normalInput,
                                   {.layout = FG::Layout::Attachment,
                                    .access = VK_ACCESS_2_SHADER_READ_BIT,
                                    .stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                    .imageAspect = VK_IMAGE_ASPECT_COLOR_BIT});

    auto normalVis = builder.create("normalDebugVis",
                                    normalInfo.size,
                                    FG::PixelFormat::RGBA32,
                                    FG::Layout::Attachment,
                                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                    VK_ACCESS_2_SHADER_WRITE_BIT);

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
    FG::Builder builder = graph.declarePass("GeneralDebug");

    auto &textureToDebug = debugTextures[debugViewIndex];
    auto dbgInfo = builder.read(textureToDebug,
                                {.layout = FG::Layout::Attachment,
                                 .access = VK_ACCESS_2_SHADER_READ_BIT,
                                 .stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                 .imageAspect = VK_IMAGE_ASPECT_DEPTH_BIT});

    auto depthVis = builder.create("debugVis",
                                   dbgInfo.size,
                                   FG::PixelFormat::RGBA32,
                                   FG::Layout::Attachment,
                                   VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                   VK_ACCESS_2_SHADER_WRITE_BIT);

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
    FG::Builder builder = graph.declarePass("MainPass");

    auto depthInfo = builder.read(depthInput,
                                  {.layout = FG::Layout::Attachment,
                                   .access = VK_ACCESS_2_SHADER_READ_BIT,
                                   .stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                   .imageAspect = VK_IMAGE_ASPECT_DEPTH_BIT});

    auto color = builder.create("colorTexture",
                                depthInfo.size,
                                FG::PixelFormat::RGBA32,
                                FG::Layout::Attachment,
                                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                VK_ACCESS_2_SHADER_WRITE_BIT);
    auto normal = builder.create("normalTexture",
                                 depthInfo.size,
                                 FG::PixelFormat::RGBA32,
                                 FG::Layout::Attachment,
                                 VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                 VK_ACCESS_2_SHADER_WRITE_BIT);

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
    FG::Builder builder = graph.declarePass("Postprocess");

    auto curColorInfo = builder.read(currentColorInput,
                                     {.layout = FG::Layout::Attachment,
                                      .access = VK_ACCESS_2_SHADER_READ_BIT,
                                      .stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                      .imageAspect = VK_IMAGE_ASPECT_COLOR_BIT});
    auto prevColorInfo = builder.read(previousColorInput,
                                      {.layout = FG::Layout::Attachment,
                                       .access = VK_ACCESS_2_SHADER_READ_BIT,
                                       .stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                       .imageAspect = VK_IMAGE_ASPECT_COLOR_BIT});

    auto color = builder.create("postprocessTexture",
                                curColorInfo.size,
                                FG::PixelFormat::RGBA32,
                                FG::Layout::Attachment,
                                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                VK_ACCESS_2_SHADER_WRITE_BIT);

    co_yield PostProcessOut{color};
    FG::RenderInput render = co_await builder.finishDeclaration();

    CO_APP_INFO("[Postprocess] Pass render commands are executed");
}
} // namespace passes

TEST_CASE("Framegraph API exploration", "[Cory/Framegraph/Framegraph]")
{
    testing::VulkanTester t;

    FG::Framegraph fg(t.ctx());
    Magnum::Vk::Image prevFrame{Corrade::NoCreate};
    FG::TextureHandle prevFrameColor =
        fg.declareInput({"previousFrameColor", glm::u32vec3{1024, 768, 1}, FG::PixelFormat::RGBA32},
                        FG::Layout::Attachment,
                        VK_ACCESS_2_SHADER_WRITE_BIT,
                        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                        prevFrame);

    auto depthPass = passes::depthPass(fg, {800, 600, 1});
    auto mainPass = passes::mainPass(fg, depthPass.output().depthTexture);

    auto depthDebugPass = passes::depthDebug(fg, depthPass.output().depthTexture);
    auto normalDebugPass = passes::normalDebug(fg, mainPass.output().normal);
    auto debugCombinePass = passes::debugGeneral(
        fg, {depthDebugPass.output().debugColor, normalDebugPass.output().debugColor}, 0);

    auto postProcess = passes::postProcess(fg, mainPass.output().color, prevFrameColor);

    // provoke the coroutines to run so we have some stuff to cull in the graph
    depthDebugPass.output();
    normalDebugPass.output();
    debugCombinePass.output();

    auto postprocessOut = postProcess.output();

    auto [resultInfo, resultState] = fg.declareOutput(postprocessOut.color);

    CO_APP_INFO("Final output is a color texture of {}x{}x{}",
                resultInfo.size.x,
                resultInfo.size.y,
                resultInfo.size.z);

    Magnum::Vk::CommandBuffer buffer = t.ctx().commandPool().allocate();

    fg.execute(buffer);
}