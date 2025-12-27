#include "TestUtils.hpp"

#include <../src/Cory/Renderer/RadixSorter.hpp>
#include <Cory/Base/ResourceLocator.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/DescriptorSets.hpp>
#include <Cory/Renderer/PipelineCache.hpp>
#include <Cory/Renderer/ShaderManager.hpp>

#include <catch2/catch_test_macros.hpp>

#include <KDGpu/buffer_options.h>
#include <KDGpu/command_recorder.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <filesystem>
#include <numeric>
#include <span>
#include <vector>

namespace fs = std::filesystem;

namespace {
std::vector<uint32_t> readbackBuffer(Gpu::Device &device,
                                     Gpu::Queue &queue,
                                     Gpu::CommandRecorder &recorder,
                                     const Gpu::Buffer &src,
                                     size_t byteSize,
                                     Gpu::AccessFlags srcMask,
                                     Gpu::PipelineStageFlags srcStages)
{
    auto readback = device.createBuffer(Gpu::BufferOptions{
        .label = "RadixSortReadback",
        .size = byteSize,
        .usage = Gpu::BufferUsageFlagBits::TransferDstBit,
        .memoryUsage = Gpu::MemoryUsage::GpuToCpu,
    });

    recorder.bufferMemoryBarrier(Gpu::BufferMemoryBarrierOptions{
        .srcStages = srcStages,
        .srcMask = srcMask,
        .dstStages = Gpu::PipelineStageFlagBit::TransferBit,
        .dstMask = Gpu::AccessFlagBit::TransferReadBit,
        .buffer = src.handle(),
    });
    recorder.copyBuffer(Gpu::BufferCopy{
        .src = src.handle(),
        .dst = readback.handle(),
        .byteSize = byteSize,
    });
    recorder.bufferMemoryBarrier(Gpu::BufferMemoryBarrierOptions{
        .srcStages = Gpu::PipelineStageFlagBit::TransferBit,
        .srcMask = Gpu::AccessFlagBit::TransferWriteBit,
        .dstStages = Gpu::PipelineStageFlagBit::HostBit,
        .dstMask = Gpu::AccessFlagBit::HostReadBit,
        .buffer = readback.handle(),
    });

    auto cmdBuf = recorder.finish();
    auto fence = device.createFence(
        Gpu::FenceOptions{.label = "Fence_RadixReadback", .createSignalled = false});
    queue.submit(Gpu::SubmitOptions{
        .commandBuffers = {cmdBuf.handle()},
        .signalFence = {fence.handle()},
    });
    fence.wait();

    auto *mapped = static_cast<uint32_t *>(readback.map());
    const size_t count = byteSize / sizeof(uint32_t);
    std::vector<uint32_t> out(mapped, mapped + count);
    readback.unmap();
    return out;
}

void uploadBuffer(Gpu::Buffer &buffer, std::span<const uint32_t> data)
{
    auto *mapped = static_cast<uint32_t *>(buffer.map());
    std::memcpy(mapped, data.data(), data.size_bytes());
    buffer.flush();
    buffer.unmap();
}
} // namespace

TEST_CASE("Radix sort compute pipeline matches CPU reference")
{
    Cory::testing::VulkanTester t;

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
        predicateBuffer.flush();
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
    recorder.bufferMemoryBarrier(Gpu::BufferMemoryBarrierOptions{
        .srcStages = Gpu::PipelineStageFlagBit::TransferBit,
        .srcMask = Gpu::AccessFlagBit::TransferWriteBit,
        .dstStages = Gpu::PipelineStageFlagBit::HostBit,
        .dstMask = Gpu::AccessFlagBit::HostReadBit,
        .buffer = readback.handle(),
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

TEST_CASE("Radix sort stages produce expected buffers for a single pass")
{
    Cory::testing::VulkanTester t;
    const auto shaderDir =
        fs::path{__FILE__}.parent_path().parent_path() / "examples/05-ParticleCompute/shaders";
    Cory::ResourceLocator::addSearchPath(shaderDir);

    auto &ctx = t.ctx();
    auto &device = ctx.device();
    Cory::RadixSorter sorter{ctx};

    const std::array<uint32_t, 8> keys = {0x1u, 0xFu, 0x2u, 0x1u, 0x0u, 0xAu, 0xFu, 0x2u};
    const uint32_t count = static_cast<uint32_t>(keys.size());
    const uint32_t bitOffset = 0u;
    auto &scratch = sorter.scratchForFrame(0, count);
    REQUIRE(scratch.workgroups == 1);

    uploadBuffer(scratch.keysA, std::span<const uint32_t>(keys));
    {
        std::array<uint32_t, keys.size()> indices{};
        std::iota(indices.begin(), indices.end(), 0u);
        uploadBuffer(scratch.indicesA, std::span<const uint32_t>(indices));
    }

    SECTION("Histogram")
    {
        auto recorder = device.createCommandRecorder();
        recorder.bufferMemoryBarrier(Gpu::BufferMemoryBarrierOptions{
            .srcStages = Gpu::PipelineStageFlagBit::HostBit,
            .srcMask = Gpu::AccessFlagBit::HostWriteBit,
            .dstStages = Gpu::PipelineStageFlagBit::ComputeShaderBit,
            .dstMask = Gpu::AccessFlagBit::ShaderStorageReadBit,
            .buffer = scratch.keysA.handle(),
        });
        sorter.dispatchHistogram(
            recorder, ctx.descriptors(), scratch, scratch.keysA, count, bitOffset, 0);

        auto histo = readbackBuffer(device,
                                    ctx.graphicsQueue(),
                                    recorder,
                                    scratch.histograms,
                                    16 * sizeof(uint32_t),
                                    Gpu::AccessFlagBit::ShaderStorageWriteBit,
                                    Gpu::PipelineStageFlagBit::ComputeShaderBit);

        std::vector<uint32_t> expected{};
        expected.resize(16);
        for (uint32_t key : keys) {
            expected[key & 0xFu] += 1;
        }
        REQUIRE(std::equal(expected.begin(), expected.end(), histo.begin()));
    }

    SECTION("Scan (upload histogram directly to isolate scan)")
    {
        auto recorder = device.createCommandRecorder();
        std::array<uint32_t, 16> counts{};
        for (uint32_t key : keys) {
            counts[key & 0xFu] += 1;
        }
        uploadBuffer(scratch.histograms, std::span<const uint32_t>(counts));
        recorder.bufferMemoryBarrier(Gpu::BufferMemoryBarrierOptions{
            .srcStages = Gpu::PipelineStageFlagBit::HostBit,
            .srcMask = Gpu::AccessFlagBit::HostWriteBit,
            .dstStages = Gpu::PipelineStageFlagBit::ComputeShaderBit,
            .dstMask = Gpu::AccessFlagBit::ShaderStorageReadBit,
            .buffer = scratch.histograms.handle(),
        });
        sorter.dispatchScan(recorder, ctx.descriptors(), scratch, 0);

        auto scanned = readbackBuffer(device,
                                      ctx.graphicsQueue(),
                                      recorder,
                                      scratch.histograms,
                                      16 * sizeof(uint32_t),
                                      Gpu::AccessFlagBit::ShaderStorageWriteBit,
                                      Gpu::PipelineStageFlagBit::ComputeShaderBit);

        std::vector<uint32_t> expected{};
        expected.resize(16);
        uint32_t sum = 0;
        for (size_t i = 0; i < expected.size(); ++i) {
            expected[i] = sum;
            sum += counts[i];
        }
        CHECK(expected == scanned);
    }

    SECTION("Scatter (upload scanned histogram directly to isolate scatter)")
    {
        auto recorder = device.createCommandRecorder();
        recorder.bufferMemoryBarrier(Gpu::BufferMemoryBarrierOptions{
            .srcStages = Gpu::PipelineStageFlagBit::HostBit,
            .srcMask = Gpu::AccessFlagBit::HostWriteBit,
            .dstStages = Gpu::PipelineStageFlagBit::ComputeShaderBit,
            .dstMask = Gpu::AccessFlagBit::ShaderStorageReadBit,
            .buffer = scratch.keysA.handle(),
        });
        recorder.bufferMemoryBarrier(Gpu::BufferMemoryBarrierOptions{
            .srcStages = Gpu::PipelineStageFlagBit::HostBit,
            .srcMask = Gpu::AccessFlagBit::HostWriteBit,
            .dstStages = Gpu::PipelineStageFlagBit::ComputeShaderBit,
            .dstMask = Gpu::AccessFlagBit::ShaderStorageReadBit,
            .buffer = scratch.indicesA.handle(),
        });
        std::array<uint32_t, 16> counts{};
        for (uint32_t key : keys) {
            counts[key & 0xFu] += 1;
        }
        std::array<uint32_t, 16> offsets{};
        uint32_t sum = 0;
        for (size_t i = 0; i < offsets.size(); ++i) {
            offsets[i] = sum;
            sum += counts[i];
        }
        uploadBuffer(scratch.histograms, std::span<const uint32_t>(offsets));
        recorder.bufferMemoryBarrier(Gpu::BufferMemoryBarrierOptions{
            .srcStages = Gpu::PipelineStageFlagBit::HostBit,
            .srcMask = Gpu::AccessFlagBit::HostWriteBit,
            .dstStages = Gpu::PipelineStageFlagBit::ComputeShaderBit,
            .dstMask = Gpu::AccessFlagBit::ShaderStorageReadBit,
            .buffer = scratch.histograms.handle(),
        });
        sorter.dispatchScatter(recorder,
                               ctx.descriptors(),
                               scratch,
                               scratch.keysA,
                               scratch.indicesA,
                               scratch.keysB,
                               scratch.indicesB,
                               count,
                               bitOffset,
                               0);

        auto keysReadback = device.createBuffer(Gpu::BufferOptions{
            .label = "RadixKeysReadback",
            .size = keys.size() * sizeof(uint32_t),
            .usage = Gpu::BufferUsageFlagBits::TransferDstBit,
            .memoryUsage = Gpu::MemoryUsage::GpuToCpu,
        });
        auto indicesReadback = device.createBuffer(Gpu::BufferOptions{
            .label = "RadixIndicesReadback",
            .size = keys.size() * sizeof(uint32_t),
            .usage = Gpu::BufferUsageFlagBits::TransferDstBit,
            .memoryUsage = Gpu::MemoryUsage::GpuToCpu,
        });

        recorder.bufferMemoryBarrier(Gpu::BufferMemoryBarrierOptions{
            .srcStages = Gpu::PipelineStageFlagBit::ComputeShaderBit,
            .srcMask = Gpu::AccessFlagBit::ShaderStorageWriteBit,
            .dstStages = Gpu::PipelineStageFlagBit::TransferBit,
            .dstMask = Gpu::AccessFlagBit::TransferReadBit,
            .buffer = scratch.keysB.handle(),
        });
        recorder.copyBuffer(Gpu::BufferCopy{
            .src = scratch.keysB.handle(),
            .dst = keysReadback.handle(),
            .byteSize = keys.size() * sizeof(uint32_t),
        });
        recorder.bufferMemoryBarrier(Gpu::BufferMemoryBarrierOptions{
            .srcStages = Gpu::PipelineStageFlagBit::ComputeShaderBit,
            .srcMask = Gpu::AccessFlagBit::ShaderStorageWriteBit,
            .dstStages = Gpu::PipelineStageFlagBit::TransferBit,
            .dstMask = Gpu::AccessFlagBit::TransferReadBit,
            .buffer = scratch.indicesB.handle(),
        });
        recorder.copyBuffer(Gpu::BufferCopy{
            .src = scratch.indicesB.handle(),
            .dst = indicesReadback.handle(),
            .byteSize = keys.size() * sizeof(uint32_t),
        });
        recorder.bufferMemoryBarrier(Gpu::BufferMemoryBarrierOptions{
            .srcStages = Gpu::PipelineStageFlagBit::TransferBit,
            .srcMask = Gpu::AccessFlagBit::TransferWriteBit,
            .dstStages = Gpu::PipelineStageFlagBit::HostBit,
            .dstMask = Gpu::AccessFlagBit::HostReadBit,
            .buffer = keysReadback.handle(),
        });
        recorder.bufferMemoryBarrier(Gpu::BufferMemoryBarrierOptions{
            .srcStages = Gpu::PipelineStageFlagBit::TransferBit,
            .srcMask = Gpu::AccessFlagBit::TransferWriteBit,
            .dstStages = Gpu::PipelineStageFlagBit::HostBit,
            .dstMask = Gpu::AccessFlagBit::HostReadBit,
            .buffer = indicesReadback.handle(),
        });

        auto cmdBuf = recorder.finish();
        auto fence = device.createFence(
            Gpu::FenceOptions{.label = "Fence_RadixScatterReadback", .createSignalled = false});
        ctx.graphicsQueue().submit(Gpu::SubmitOptions{
            .commandBuffers = {cmdBuf.handle()},
            .signalFence = {fence.handle()},
        });
        fence.wait();

        auto *mappedKeys = static_cast<uint32_t *>(keysReadback.map());
        std::vector<uint32_t> keysOut(mappedKeys, mappedKeys + keys.size());
        keysReadback.unmap();
        auto *mappedIndices = static_cast<uint32_t *>(indicesReadback.map());
        std::vector<uint32_t> indicesOut(mappedIndices, mappedIndices + keys.size());
        indicesReadback.unmap();

        std::vector<uint32_t> expectedKeys(keys.size());
        std::vector<uint32_t> expectedIndices(keys.size());
        std::array<uint32_t, 16> local{};
        for (size_t i = 0; i < keys.size(); ++i) {
            const uint32_t digit = keys[i] & 0xFu;
            const uint32_t dst = offsets[digit] + local[digit]++;
            expectedKeys[dst] = keys[i];
            expectedIndices[dst] = static_cast<uint32_t>(i);
        }

        CHECK(keysOut == expectedKeys);
        CHECK(indicesOut == expectedIndices);
    }
}
