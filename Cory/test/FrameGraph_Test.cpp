#include <catch2/catch.hpp>

#include <Cory/Base/FmtUtils.hpp>
#include <Cory/Framegraph/Framegraph.hpp>
#include <Cory/Framegraph/RenderPassDeclaration.hpp>

#include <cppcoro/fmap.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/when_all.hpp>

using namespace Cory;
namespace FG = Cory::Framegraph;

struct DepthPassOutputs {
    FG::TextureHandle depthTexture;
};
FG::RenderPassDeclaration<DepthPassOutputs> depthPass(FG::Framegraph &graph, glm::u32vec3 size)
{
    FG::Builder builder = graph.declarePass("DepthPrepass");

    DepthPassOutputs outputs{
        .depthTexture = co_await builder.create(
            "depthTexture", size, FG::PixelFormat::D32, FG::Layout::DepthStencilAttachment)};

    co_yield outputs;
    FG::RenderInput render = co_await builder.finishDeclaration();

    CO_APP_INFO("DepthPrepass render commands executing");
    
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
        "colorTexture", depth.size, FG::PixelFormat::RGBA32, FG::Layout::ColorAttachment);
    FG::MutableTextureHandle normalWritable = co_await builder.create(
        "normalTexture", depth.size, FG::PixelFormat::RGBA32, FG::Layout::ColorAttachment);

    co_yield MainOut{colorWritable, normalWritable};
    FG::RenderInput render = co_await builder.finishDeclaration();

    // render portion
    struct PassTextures {
        FG::Texture color;
        FG::Texture depth;
        FG::Texture normal;
    };

    CO_APP_INFO("Rendering main pass");
}

TEST_CASE("Framegraph API exploration", "[Cory/Framegraph/Framegraph]")
{
    FG::Framegraph fg;
    auto depthPassTask = depthPass(fg, {800, 600, 1});
    auto mainPassTask = mainPass(fg, depthPassTask.output().depthTexture);

    auto mainOut = mainPassTask.output();
    CO_APP_INFO("Main pass outputs a color texture of {}x{}x{}",
                mainOut.color.size.x,
                mainOut.color.size.y,
                mainOut.color.size.z);

    fg.execute();
}