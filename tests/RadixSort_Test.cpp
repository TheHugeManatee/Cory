#include "TestUtils.hpp"

#include <Cory/Base/ResourceLocator.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/DescriptorSets.hpp>
#include <Cory/Renderer/PipelineCache.hpp>
#include <Cory/Renderer/ShaderManager.hpp>
#include <RadixSorter.hpp>

#include <catch2/catch_test_macros.hpp>

#include <glm/vec3.hpp>

#include <KDGpu/buffer_options.h>

#include <algorithm>
#include <filesystem>
#include <numeric>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr uint32_t kWorkgroupSize = 256u;

} // namespace

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

    struct GPUInstanceData {
        glm::vec4 positionAndSize;
        glm::vec4 color;
        glm::vec4 parameters;
    };
    std::vector<GPUInstanceData> instances;
    instances.reserve(distances.size());
    for (float d : distances) {
        instances.push_back({
            .positionAndSize = glm::vec4{0.0f, 0.0f, -d, 0.5f},
            .color = glm::vec4{1.0f},
            .parameters = glm::vec4{0.0f},
        });
    }

    struct GPUUBO {
        glm::mat4 projection;
        glm::mat4 view;
        glm::mat4 viewProjection;
        glm::vec3 lightPosition;
        float pad0{};
        glm::vec3 cameraRight;
        float pad1{};
        glm::vec3 cameraUp;
        float pad2{};
    } uboData{};
    uboData.view = glm::mat4{1.0f};
    uboData.projection = glm::mat4{1.0f};
    uboData.viewProjection = glm::mat4{1.0f};
    uboData.cameraRight = {1.0f, 0.0f, 0.0f};
    uboData.cameraUp = {0.0f, 1.0f, 0.0f};

    auto makeBuffer = [&](std::string_view label, Gpu::DeviceSize size) {
        return device.createBuffer(Gpu::BufferOptions{
            .label = label,
            .size = size,
            .usage = Gpu::BufferUsageFlagBits::StorageBufferBit,
            .memoryUsage = Gpu::MemoryUsage::CpuToGpu,
        });
    };

    auto instanceBuffer = makeBuffer("TestInstances", instances.size() * sizeof(GPUInstanceData));
    auto ubo = makeBuffer("UBO", sizeof(GPUUBO));

    // Upload data
    {
        auto *mapped = static_cast<std::byte *>(instanceBuffer.map());
        std::memcpy(mapped, instances.data(), instances.size() * sizeof(GPUInstanceData));
        instanceBuffer.unmap();
    }
    {
        auto *mapped = static_cast<GPUUBO *>(ubo.map());
        *mapped = uboData;
        ubo.unmap();
    }

    auto &scratch = sorter.scratchForFrame(0, count);

    auto recorder = device.createCommandRecorder();
    auto &sorted = sorter.sort(recorder, ctx.descriptors(), scratch, instanceBuffer, ubo, count, 0);

    auto cmdBuf = recorder.finish();
    auto fence = device.createFence();
    ctx.graphicsQueue().submit(Gpu::SubmitOptions{
        .commandBuffers = {cmdBuf.handle()},
        .signalFence = {fence.handle()},
    });
    fence.wait();

    auto *mappedIndices = static_cast<uint32_t *>(sorted.map());
    std::vector<uint32_t> gpuSorted(mappedIndices, mappedIndices + count);
    sorted.unmap();

    // CPU expected far-to-near
    std::vector<uint32_t> expected(count);
    std::iota(expected.begin(), expected.end(), 0u);
    std::stable_sort(expected.begin(), expected.end(), [&](uint32_t a, uint32_t b) {
        return distances[a] > distances[b];
    });

    REQUIRE(gpuSorted == expected);
}