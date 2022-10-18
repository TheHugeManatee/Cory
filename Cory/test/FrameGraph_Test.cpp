#include <catch2/catch.hpp>

#include <Cory/Base/FmtUtils.hpp>
#include <Cory/Framegraph/Framegraph.hpp>

#include <cppcoro/sync_wait.hpp>
#include <cppcoro/when_all.hpp>
using namespace Cory;
namespace FG = Cory::Framegraph;

/*TODO*/ cppcoro::task<FG::TextureHandle> depthPass(FG::Framegraph &graph, glm::u32vec3 size)
{
    FG::Builder builder = graph.declarePass("DepthPrepass");

    FG::MutableTextureHandle depthInputHandle = co_await builder.create(
        "depthTexture", size, FG::PixelFormat::D32, FG::Layout::DepthStencilAttachment);

    co_return FG::TextureHandle{
        .name = depthInputHandle.name,
        .size = depthInputHandle.size,
        .format = depthInputHandle.format,
        .layout = depthInputHandle.layout,
        .resource = [](auto depthInputHandle) -> cppcoro::shared_task<FG::Texture> {
            auto render_to = co_await depthInputHandle.resource;
            CO_APP_INFO("Rendering depth pass");
            // TODO render pass and commands
            co_return render_to;
        }(depthInputHandle)};
}

struct MainOut {
    FG::TextureHandle color;
    FG::TextureHandle depth;
    FG::TextureHandle normal;
};
/*TODO*/ cppcoro::task<MainOut> mainPass(FG::Framegraph graph,
                                         cppcoro::task<FG::TextureHandle> &depthInput)
{
    FG::Builder builder = graph.declarePass("MainPass");
    auto depthHandle = co_await depthInput;

    FG::TextureHandle depth = co_await builder.read(depthHandle);
    FG::MutableTextureHandle colorWritable = co_await builder.create(
        "colorTexture", depth.size, FG::PixelFormat::RGBA32, FG::Layout::ColorAttachment);
    FG::MutableTextureHandle normalWritable = co_await builder.create(
        "normalTexture", depth.size, FG::PixelFormat::RGBA32, FG::Layout::ColorAttachment);

    struct ResultTextures {
        FG::Texture color;
        FG::Texture depth;
        FG::Texture normal;
    };

    auto render_result = [](auto colorWritable,
                            auto depth,
                            auto normalWritable) -> cppcoro::shared_task<ResultTextures> {
        // wait until the textures are allocated and available
        auto [colorTex, depthTex, normalTex] = co_await cppcoro::when_all(
            colorWritable.resource, depth.resource, normalWritable.resource);
        CO_CORE_INFO("Rendering main pass");
        // TODO render pass and commands
        co_return ResultTextures{colorTex, depthTex, normalTex};
    }(colorWritable, depth, normalWritable);

    co_return MainOut{{.name = colorWritable.name,
                       .size = colorWritable.size,
                       .format = colorWritable.format,
                       .layout = colorWritable.layout,
                       .resource = [](auto render_result) -> cppcoro::shared_task<FG::Texture> {
                           auto rendered = co_await render_result;
                           co_return rendered.color;
                       }(render_result)},
                      depth,
                      {.name = normalWritable.name,
                       .size = normalWritable.size,
                       .format = normalWritable.format,
                       .layout = normalWritable.layout,
                       .resource = [](auto render_result) -> cppcoro::shared_task<FG::Texture> {
                           auto rendered = co_await render_result;
                           co_return rendered.normal;
                       }(render_result)}};
}

TEST_CASE("Framegraph API exploration", "[Cory/Framegraph/Framegraph]")
{
    Cory::Log::Init();

    FG::Framegraph fg;
    CO_CORE_INFO("Framegraph setup");

    auto depthHandle = depthPass(fg, {800, 600, 1});
    auto colorOut = mainPass(fg, depthHandle);

    CO_CORE_INFO("Request final image handle");
    FG::TextureHandle finalColor = cppcoro::sync_wait(colorOut).color;

    CO_CORE_INFO("Request final rendered image");
    FG::Texture colorTexture = cppcoro::sync_wait(finalColor.resource);

    CO_CORE_INFO("Rendering finished");
}