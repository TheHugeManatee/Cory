#include "TestUtils.hpp"

#include <Cory/Base/ResourceLocator.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/DescriptorSets.hpp>
#include <Cory/Renderer/PipelineCache.hpp>
#include <Cory/Renderer/ShaderManager.hpp>
#include <RadixSorter.hpp>

#include <catch2/catch_test_macros.hpp>

#include <KDGpu/buffer_options.h>
#include <KDGpu/command_recorder.h>

#include <algorithm>
#include <bit>
#include <cstring>
#include <filesystem>
#include <numeric>
#include <vector>

namespace fs = std::filesystem;

TEST_CASE("Radix sort compute pipeline matches CPU reference")
{
    Cory::testing::VulkanTester t;
    const auto shaderDir =
        fs::path{__FILE__}.parent_path().parent_path() / "examples/05-ParticleCompute/shaders";
    Cory::ResourceLocator::addSearchPath(shaderDir);

    auto &ctx = t.ctx();
    auto &device = ctx.device();
    Cory::RadixSorter sorter{ctx};

    // Toy distances (larger = farther)
    std::vector<float> distances{5.0f, 1.0f, 3.5f, 8.0f, 0.5f, 2.5f, 7.0f, 4.0f};
    const uint32_t count = gsl::narrow<uint32_t>(distances.size());
    std::vector<uint32_t> keys;
    keys.reserve(distances.size());
    for (float d : distances) {
        const float clamped = std::max(0.0f, d);
        uint32_t key = std::bit_cast<uint32_t>(clamped);
        keys.push_back(~key);
    }

    auto makeBuffer =
        [&](std::string_view label, Gpu::DeviceSize size, Gpu::BufferUsageFlags usage) {
            return device.createBuffer(Gpu::BufferOptions{
                .label = label,
                .size = size,
                .usage = usage,
                .memoryUsage = Gpu::MemoryUsage::CpuToGpu,
            });
        };

    auto predicateBuffer = makeBuffer("PredicateValues",
                                      keys.size() * sizeof(uint32_t),
                                      Gpu::BufferUsageFlagBits::StorageBufferBit |
                                          Gpu::BufferUsageFlagBits::TransferSrcBit);
    auto sortedIndicesBuffer = makeBuffer("SortedIndices",
                                          keys.size() * sizeof(uint32_t),
                                          Gpu::BufferUsageFlagBits::StorageBufferBit |
                                              Gpu::BufferUsageFlagBits::TransferSrcBit |
                                              Gpu::BufferUsageFlagBits::TransferDstBit);

    {
        auto *mapped = static_cast<uint32_t *>(predicateBuffer.map());
        std::memcpy(mapped, keys.data(), keys.size() * sizeof(uint32_t));
        predicateBuffer.unmap();
    }

    auto &scratch = sorter.scratchForFrame(0, count);

    auto recorder = device.createCommandRecorder();
    auto &sorted = sorter.sort(
        recorder, ctx.descriptors(), scratch, predicateBuffer, count, sortedIndicesBuffer, 0);

    auto readback = device.createBuffer(Gpu::BufferOptions{
        .label = "SortedReadback",
        .size = keys.size() * sizeof(uint32_t),
        .usage = Gpu::BufferUsageFlagBits::TransferDstBit,
        .memoryUsage = Gpu::MemoryUsage::GpuToCpu,
    });

    recorder.bufferMemoryBarrier(Gpu::BufferMemoryBarrierOptions{
        .srcStages = Gpu::PipelineStageFlagBit::TransferBit,
        .srcMask = Gpu::AccessFlagBit::TransferWriteBit,
        .dstStages = Gpu::PipelineStageFlagBit::TransferBit,
        .dstMask = Gpu::AccessFlagBit::TransferReadBit,
        .buffer = sorted.handle(),
    });
    recorder.copyBuffer(Gpu::BufferCopy{
        .src = sorted.handle(),
        .dst = readback.handle(),
        .byteSize = keys.size() * sizeof(uint32_t),
    });

    auto cmdBuf = recorder.finish();
    auto fence = device.createFence(
        Gpu::FenceOptions{.label = "Fence_ShadersFinished", .createSignalled = false});
    ctx.graphicsQueue().submit(Gpu::SubmitOptions{
        .commandBuffers = {cmdBuf.handle()},
        .signalFence = {fence.handle()},
    });
    fence.wait();

    auto *mappedIndices = static_cast<uint32_t *>(readback.map());
    std::vector<uint32_t> gpuSorted(mappedIndices, mappedIndices + count);
    readback.unmap();

    // CPU expected far-to-near
    std::vector<uint32_t> expected(count);
    std::iota(expected.begin(), expected.end(), 0u);
    std::stable_sort(expected.begin(), expected.end(), [&](uint32_t a, uint32_t b) {
        return distances[a] > distances[b];
    });

    REQUIRE(gpuSorted == expected);
}
