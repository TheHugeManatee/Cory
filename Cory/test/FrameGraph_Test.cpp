#include <catch2/catch.hpp>

#include <Cory/Base/FmtUtils.hpp>
#include <Cory/Framegraph/Framegraph.hpp>

#include <cppcoro/fmap.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/when_all.hpp>

using namespace Cory;
namespace FG = Cory::Framegraph;


cppcoro::shared_task<FG::TextureHandle> depthPass(FG::Framegraph &graph, glm::u32vec3 size)
{
    FG::Builder builder = graph.declarePass("DepthPrepass");

    FG::MutableTextureHandle depthInputHandle = co_await builder.create(
        "depthTexture", size, FG::PixelFormat::D32, FG::Layout::DepthStencilAttachment);

    co_return performWrite(depthInputHandle,
                           [](auto depthInputHandle) -> cppcoro::shared_task<FG::Texture> {
                               auto render_to = co_await depthInputHandle.resource;
                               CO_APP_INFO("Rendering depth pass");
                               // TODO render pass and commands
                               co_return render_to;
                           });
}

struct MainOut {
    cppcoro::shared_task<FG::TextureHandle> color;
    cppcoro::shared_task<FG::TextureHandle> normal;
};
cppcoro::shared_task<MainOut> mainPass(FG::Framegraph graph,
                                       cppcoro::shared_task<FG::TextureHandle> &depthInput)
{
    FG::Builder builder = graph.declarePass("MainPass");

    // setup portion
    auto depthHandle = co_await depthInput;

    FG::TextureHandle depth = co_await builder.read(depthHandle);
    FG::MutableTextureHandle colorWritable = co_await builder.create(
        "colorTexture", depth.size, FG::PixelFormat::RGBA32, FG::Layout::ColorAttachment);
    FG::MutableTextureHandle normalWritable = co_await builder.create(
        "normalTexture", depth.size, FG::PixelFormat::RGBA32, FG::Layout::ColorAttachment);

    // render portion
    struct PassTextures {
        FG::Texture color;
        FG::Texture depth;
        FG::Texture normal;
    };

    auto renderTask = [](auto colorWritable,
                         auto depth,
                         auto normalWritable) -> cppcoro::shared_task<PassTextures> {
        // wait until the textures are allocated and available
        auto [colorTex, depthTex, normalTex] = co_await cppcoro::when_all(
            colorWritable.resource, depth.resource, normalWritable.resource);
        CO_APP_INFO("Rendering main pass");
        // TODO render pass and commands
        co_return PassTextures{colorTex, depthTex, normalTex};
    }(colorWritable, depth, normalWritable);

    auto renderedColor = performWriteAsync(
        colorWritable,
        [](auto handle, auto renderTask) -> cppcoro::shared_task<FG::Texture> {
            co_return (co_await renderTask).color;
        },
        renderTask);

    auto renderedNormal = performWriteAsync(
        colorWritable,
        [](auto handle, auto renderTask) -> cppcoro::shared_task<FG::Texture> {
            co_return (co_await renderTask).depth;
        },
        renderTask);

    co_return MainOut{renderedColor, renderedNormal};
}

TEST_CASE("Framegraph API exploration", "[Cory/Framegraph/Framegraph]")
{
    Cory::Log::Init();

    FG::Framegraph fg;

    auto buildRenderGraph = [&]() -> cppcoro::task<void> {
        CO_APP_INFO("=== Setup passes and dependencies ===");
        auto depthPassTask = depthPass(fg, {800, 600, 1});
        auto mainPassTask = mainPass(fg, depthPassTask);

        CO_APP_INFO("=== Request final image handle ===");
        auto [depthPassOutput, mainPassOutput] =
            co_await cppcoro::when_all(depthPassTask, mainPassTask);
        FG::TextureHandle finalColor = co_await mainPassOutput.color;

        CO_APP_INFO("=== Request final rendered image ===");
        FG::Texture colorTexture = co_await finalColor.resource;
    };

    auto finalTexture = buildRenderGraph();
    cppcoro::sync_wait(finalTexture);
    CO_APP_INFO("=== Rendering finished ===");
}