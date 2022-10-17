#include <catch2/catch.hpp>

#include <Cory/Framegraph/Framegraph.hpp>

#include <cppcoro/sync_wait.hpp>

using namespace Cory;
namespace FG = Cory::Framegraph;

/*TODO*/ cppcoro::task<FG::TextureHandle> depthPass(FG::Framegraph &graph, glm::u32vec3 size)
{
    FG::Builder &builder = graph.declarePass("DepthPrepass");

    std::string_view renderPassHandle{"depth"};
    FG::MutableTextureHandle depthHandle =
        co_await builder.create("depth", size, {/* D32 */}, {/* CAO */});

    auto rsrc = co_await builder.acquireResources(depthHandle);

    co_return depthHandle;

    // TODO: define render pass
    // TODO: record commands
}

/*TODO*/ cppcoro::task<void> mainPass(FG::Framegraph graph, FG::TextureHandle depthHandle)
{
    FG::Builder &builder = graph.declarePass("MainPass");
    FG::TextureHandle depth = co_await builder.read("depth", depthHandle);

    FG::MutableTextureHandle color =
        co_await builder.create("color", depth.size, {/* RGBA32F */}, {/* CAO */});
    FG::MutableTextureHandle normal =
        co_await builder.create("normal", depth.size, {/* RGBA32F */}, {/* CAO */});

    co_return color;
    // TODO: define render pass
    // TODO: record commands
}

// cppcoro::task<void> postprocess(FG::Builder builder)
//{
//     // inputs
//     FG::TextureHandle color = co_await builder.read("color", {/* TODO */});
//     FG::TextureHandle depth = co_await builder.read("depth", {/* TODO */});
//
//     // output
//     FG::MutableTextureHandle final = co_await builder.create("final", {/* TODO */});
//
//     // TODO: define render pass
//     // TODO: record commands
// }

TEST_CASE("Framegraph API exploration", "[Cory/Framegraph/Framegraph]")
{
    FG::Framegraph fg;

    auto build_task = fg.build([&](FG::Builder &builder) -> cppcoro::task<void> {
        auto depthHandle = co_await depthPass(fg, {800, 600, 1});

        auto colorHandle = co_await mainPass(fg, depthHandle);

        co_return;
    });

    cppcoro::sync_wait(build_task);
}