#include <catch2/catch_test_macros.hpp>

#include <Cory/Base/FmtUtils.hpp>
#include <Cory/Framegraph/Framegraph.hpp>
#include <Cory/Framegraph/RenderPassDeclaration.hpp>

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
    FG::Builder builder = graph.declarePass("DepthPrepass");

    DepthPassOutputs outputs{
        .depthTexture = co_await builder.create(
            "depthTexture", size, FG::PixelFormat::D32, FG::Layout::DepthStencil)};

    co_yield outputs;
    FG::RenderInput render = co_await builder.finishDeclaration();

    CO_APP_INFO("[DepthPrepass] render commands executing");
}

struct DepthDebugOut {
    FG::TextureHandle debugColor;
};
FG::RenderPassDeclaration<DepthDebugOut> depthDebug(FG::Framegraph &graph,
                                                    FG::TextureHandle depthInput)
{
    FG::Builder builder = graph.declarePass("DepthDebug");

    FG::TextureHandle depth = co_await builder.read(depthInput);
    FG::MutableTextureHandle depthVis = co_await builder.create(
        "depthDebugVis", depth.size, FG::PixelFormat::RGBA32, FG::Layout::Color);

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

    FG::TextureHandle normal = co_await builder.read(normalInput);
    FG::MutableTextureHandle normalVis = co_await builder.create(
        "normalDebugVis", normalInput.size, FG::PixelFormat::RGBA32, FG::Layout::Color);

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
    FG::TextureHandle depth = co_await builder.read(textureToDebug);

    FG::MutableTextureHandle depthVis = co_await builder.create(
        "debugVis", textureToDebug.size, FG::PixelFormat::RGBA32, FG::Layout::Color);

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

    FG::TextureHandle depth = co_await builder.read(depthInput);
    FG::MutableTextureHandle colorWritable = co_await builder.create(
        "colorTexture", depth.size, FG::PixelFormat::RGBA32, FG::Layout::Color);
    FG::MutableTextureHandle normalWritable = co_await builder.create(
        "normalTexture", depth.size, FG::PixelFormat::RGBA32, FG::Layout::Color);

    co_yield MainOut{colorWritable, normalWritable};
    FG::RenderInput render = co_await builder.finishDeclaration();

    // render portion
    struct PassTextures {
        FG::Texture color;
        FG::Texture depth;
        FG::Texture normal;
    };

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

    FG::TextureHandle curColor = co_await builder.read(currentColorInput);
    FG::TextureHandle prevColor = co_await builder.read(previousColorInput);
    FG::MutableTextureHandle colorWritable = co_await builder.create(
        "postprocessTexture", curColor.size, FG::PixelFormat::RGBA32, FG::Layout::Color);

    co_yield PostProcessOut{colorWritable};
    FG::RenderInput render = co_await builder.finishDeclaration();

    CO_APP_INFO("[Postprocess] Pass render commands are executed");
}
} // namespace passes

TEST_CASE("Framegraph API exploration", "[Cory/Framegraph/Framegraph]")
{
    FG::Framegraph fg;
    FG::TextureHandle prevFrameColor = fg.declareInput("previousFrameColor");

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

    fg.declareOutput(postprocessOut.color);

    CO_APP_INFO("Final output is a color texture of {}x{}x{}",
                postprocessOut.color.size.x,
                postprocessOut.color.size.y,
                postprocessOut.color.size.z);


    fg.execute();
}