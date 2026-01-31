#include "TestUtils.hpp"

#include <../src/Cory/Renderer/RadixSorter.hpp>
#include <Cory/Base/ResourceLocator.hpp>
#include <Cory/Framegraph/Framegraph.hpp>
#include <Cory/Framegraph/FramegraphResourceManager.hpp>
#include <Cory/Framegraph/RenderTaskBuilder.hpp>
#include <Cory/Framegraph/RenderTaskDeclaration.hpp>
#include <Cory/Renderer/Context.hpp>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>
#include <unordered_map>

namespace fs = std::filesystem;

namespace {
class FramegraphTestAdapter : public Cory::Framegraph {
  public:
    using Framegraph::compile;
    using Framegraph::Framegraph;
};

struct SortTaskOut {
    Cory::TransientBufferHandle indices;
};

Cory::RenderTaskDeclaration<Cory::TransientBufferHandle>
writePredicateTask(Cory::RenderTaskBuilder builder, Cory::TransientBufferHandle keys)
{
    auto [writtenKeys, info] = builder.write(
        keys, Gpu::BufferUsageFlagBits::StorageBufferBit, Cory::Sync::AccessType::HostWrite);
    (void)info;
    [[maybe_unused]] Cory::RenderInput render = co_await builder.finishDeclaration(writtenKeys);
}

Cory::RenderTaskDeclaration<SortTaskOut> framegraphSortTask(Cory::RenderTaskBuilder builder,
                                                            Cory::RadixSorter &sorter)
{
    auto keys = builder.create("BUF_FramegraphSortKeys",
                               256u,
                               Gpu::BufferUsageFlagBits::StorageBufferBit,
                               Cory::Sync::AccessType::HostWrite,
                               Gpu::MemoryUsage::CpuToGpu);
    auto keysWritten = writePredicateTask(builder.subtask("PreprocessKeys"), keys);
    auto sorted = sorter.sort(builder, keysWritten.output(), 64u);

    [[maybe_unused]] Cory::RenderInput render =
        co_await builder.finishDeclaration(SortTaskOut{sorted.indices});
}

Cory::RenderTaskDeclaration<Cory::TransientTextureHandle>
sortSinkTask(Cory::RenderTaskBuilder builder, Cory::TransientBufferHandle indices)
{
    builder.read(indices,
                 Gpu::BufferUsageFlagBits::StorageBufferBit,
                 Cory::Sync::AccessType::ComputeShaderReadOther);
    auto color = builder.create("TEX_RadixSortSink",
                                {1, 1, 1},
                                Cory::TextureFormat::R8G8B8A8_SRGB,
                                Gpu::TextureUsageFlagBits::ColorAttachmentBit,
                                Cory::Sync::AccessType::ColorAttachmentWrite);
    [[maybe_unused]] Cory::RenderInput render = co_await builder.finishDeclaration(color);
}
} // namespace

TEST_CASE("Radix sorter can be scheduled via framegraph")
{
    Cory::testing::VulkanTester t;
    auto &ctx = t.ctx();
    Cory::FramegraphResourceManager graphResources(ctx);
    FramegraphTestAdapter graph(ctx, graphResources, 0);

    const auto shaderDir = fs::path{__FILE__}.parent_path().parent_path() / "data/shaders";
    Cory::ResourceLocator::addSearchPath(shaderDir);

    Cory::RadixSorter sorter{ctx};
    auto sortTask = framegraphSortTask(graph.declareTask("TASK_RadixSort"), sorter);
    auto sinkTask =
        sortSinkTask(graph.declareTask("TASK_RadixSortSink"), sortTask.output().indices);

    graph.declareOutput(sinkTask.output(), Cory::Sync::AccessType::ColorAttachmentWrite);

    auto execInfo = graph.compile();
    CHECK(!execInfo.tasks.empty());

    std::unordered_map<Cory::RenderTaskHandle, std::string> names;
    for (const auto &[handle, info] : graph.renderTasks()) {
        names.emplace(handle, info.name);
    }

    auto taskIndex = [&](std::string_view name) {
        for (size_t i = 0; i < execInfo.tasks.size(); ++i) {
            if (names[execInfo.tasks[i]] == name) {
                return i;
            }
        }
        FAIL("Task not found");
        return execInfo.tasks.size();
    };

    CHECK(taskIndex("TASK_RadixSort::RadixInitIndices") <
          taskIndex("TASK_RadixSort::RadixHistogram_0"));
    CHECK(taskIndex("TASK_RadixSort::PreprocessKeys") <
          taskIndex("TASK_RadixSort::RadixHistogram_0"));
    CHECK(taskIndex("TASK_RadixSort::RadixScatter_0") <
          taskIndex("TASK_RadixSort::RadixHistogram_4"));
    CHECK(taskIndex("TASK_RadixSort::RadixScatter_28") < taskIndex("TASK_RadixSortSink"));
}
