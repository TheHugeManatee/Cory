#include "RadixSorter.hpp"

#include <Cory/Base/ResourceLocator.hpp>
#include <Cory/Framegraph/FramegraphResourceManager.hpp>
#include <Cory/Framegraph/RenderTaskBuilder.hpp>
#include <Cory/Framegraph/RenderTaskDeclaration.hpp>
#include <Cory/Framegraph/ShaderBindingContext.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/ShaderManager.hpp>

#include <KDGpu/vulkan/vulkan_buffer.h>

#include <numeric>
#include <utility>

namespace Cory {
namespace {
constexpr uint32_t kWorkgroupSize = 256u;

ShaderHandle createComputeShader(Context &ctx, std::string_view path)
{
    ShaderSource source{ResourceLocator::Locate(path)};
    return ctx.shaders().createShader(std::move(source));
}

struct ScatterOutput {
    TransientBufferHandle keys;
    TransientBufferHandle indices;
};

struct RadixSortBuffers {
    TransientBufferHandle keysB;
    TransientBufferHandle indicesA;
    TransientBufferHandle indicesB;
    TransientBufferHandle histograms;
};

RenderTaskDeclaration<RadixSortBuffers>
radixSetupTask(RenderTaskBuilder builder, Gpu::DeviceSize required, Gpu::DeviceSize histogramSize)
{
    auto keysB = builder.create("BUF_RadixSortKeysB",
                                required,
                                Gpu::BufferUsageFlagBits::StorageBufferBit |
                                    Gpu::BufferUsageFlagBits::ShaderDeviceAddressBit,
                                Sync::AccessType::ComputeShaderWrite);
    auto indicesA = builder.create("BUF_RadixSortIndicesA",
                                   required,
                                   Gpu::BufferUsageFlagBits::StorageBufferBit |
                                       Gpu::BufferUsageFlagBits::ShaderDeviceAddressBit,
                                   Sync::AccessType::HostWrite,
                                   Gpu::MemoryUsage::CpuToGpu);
    auto indicesB = builder.create("BUF_RadixSortIndicesB",
                                   required,
                                   Gpu::BufferUsageFlagBits::StorageBufferBit |
                                       Gpu::BufferUsageFlagBits::ShaderDeviceAddressBit,
                                   Sync::AccessType::ComputeShaderWrite);
    auto histograms = builder.create("BUF_RadixSortHistograms",
                                     histogramSize,
                                     Gpu::BufferUsageFlagBits::StorageBufferBit |
                                         Gpu::BufferUsageFlagBits::ShaderDeviceAddressBit,
                                     Sync::AccessType::ComputeShaderWrite);
    [[maybe_unused]] RenderInput render = co_await builder.finishDeclaration(RadixSortBuffers{
        .keysB = keysB,
        .indicesA = indicesA,
        .indicesB = indicesB,
        .histograms = histograms,
    });
}

RenderTaskDeclaration<TransientBufferHandle> radixInitIndicesTask(
    RenderTaskBuilder builder, TransientBufferHandle indicesHandle, uint32_t instanceCount)
{
    auto [writtenIndices, info] =
        builder.write(indicesHandle,
                      Gpu::BufferUsageFlagBits::StorageBufferBit |
                          Gpu::BufferUsageFlagBits::ShaderDeviceAddressBit,
                      Sync::AccessType::HostWrite);
    (void)info;

    RenderInput renderApi = co_await builder.finishDeclaration(writtenIndices);

    auto [bufferHandle, buffer] = renderApi.resources->bufferResource(writtenIndices);
    (void)bufferHandle;
    auto *mapped = static_cast<uint32_t *>(buffer->map());
    std::iota(mapped, mapped + instanceCount, 0u);
    buffer->unmap();
}

RenderTaskDeclaration<TransientBufferHandle>
radixHistogramTask(RenderTaskBuilder builder,
                   TransientBufferHandle keysHandle,
                   TransientBufferHandle indicesHandle,
                   TransientBufferHandle histogramsHandle,
                   uint32_t instanceCount,
                   uint32_t workgroups,
                   uint32_t bitOffset,
                   ShaderHandle histogramShader)
{
    builder.read(keysHandle,
                 Gpu::BufferUsageFlagBits::StorageBufferBit |
                     Gpu::BufferUsageFlagBits::ShaderDeviceAddressBit,
                 Sync::AccessType::ComputeShaderReadOther);
    // Ensure each radix step depends on the previous scatter's outputs.
    builder.read(indicesHandle,
                 Gpu::BufferUsageFlagBits::StorageBufferBit |
                     Gpu::BufferUsageFlagBits::ShaderDeviceAddressBit,
                 Sync::AccessType::ComputeShaderReadOther);
    auto [writtenHistograms, info] =
        builder.write(histogramsHandle,
                      Gpu::BufferUsageFlagBits::StorageBufferBit |
                          Gpu::BufferUsageFlagBits::ShaderDeviceAddressBit,
                      Sync::AccessType::ComputeShaderWrite);
    (void)info;

    auto pass = builder.declareComputePass(ComputePassDeclaration{
        .name = fmt::format("PASS_RadixHistogram_{}", bitOffset),
        .shader = histogramShader,
    });

    RenderInput renderApi = co_await builder.finishDeclaration(writtenHistograms);

    auto recorder = pass.begin(renderApi);
    recorder.bindShader(renderApi.ctx->shaders()[histogramShader].shaderHandle());

    struct RadixHistogramPushConstants {
        uint32_t numInstances;
        uint32_t bitOffset;
        BufferDeviceAddress keys;
        BufferDeviceAddress histograms;
    } pc{instanceCount,
         bitOffset,
         renderApi.resources->deviceAddress(keysHandle),
         renderApi.resources->deviceAddress(writtenHistograms)};

    renderApi.bindingContext->push(pc);
    recorder.dispatchCompute({workgroups, 1, 1});
    pass.end(std::move(recorder));
}

RenderTaskDeclaration<TransientBufferHandle> radixScanTask(RenderTaskBuilder builder,
                                                           TransientBufferHandle histogramsHandle,
                                                           uint32_t workgroups,
                                                           uint32_t bitOffset,
                                                           ShaderHandle scanShader)
{
    auto [writtenHistograms, info] =
        builder.readWrite(histogramsHandle,
                          Gpu::BufferUsageFlagBits::StorageBufferBit |
                              Gpu::BufferUsageFlagBits::ShaderDeviceAddressBit,
                          Sync::AccessType::ComputeShaderWrite);
    (void)info;

    auto pass = builder.declareComputePass(ComputePassDeclaration{
        .name = fmt::format("PASS_RadixScan_{}", bitOffset),
        .shader = scanShader,
    });

    RenderInput renderApi = co_await builder.finishDeclaration(writtenHistograms);

    auto recorder = pass.begin(renderApi);
    recorder.bindShader(renderApi.ctx->shaders()[scanShader].shaderHandle());

    struct RadixScanPushConstants {
        uint32_t numWorkgroups;
        uint32_t pad;
        BufferDeviceAddress histograms;
    } pc{workgroups, 0u, renderApi.resources->deviceAddress(writtenHistograms)};

    renderApi.bindingContext->push(pc);
    recorder.dispatchCompute({1, 1, 1});
    pass.end(std::move(recorder));
}

RenderTaskDeclaration<ScatterOutput> radixScatterTask(RenderTaskBuilder builder,
                                                      TransientBufferHandle keysIn,
                                                      TransientBufferHandle indicesIn,
                                                      TransientBufferHandle keysOut,
                                                      TransientBufferHandle indicesOut,
                                                      TransientBufferHandle histogramsHandle,
                                                      uint32_t instanceCount,
                                                      uint32_t workgroups,
                                                      uint32_t bitOffset,
                                                      ShaderHandle scatterShader)
{
    builder.read(keysIn,
                 Gpu::BufferUsageFlagBits::StorageBufferBit |
                     Gpu::BufferUsageFlagBits::ShaderDeviceAddressBit,
                 Sync::AccessType::ComputeShaderReadOther);
    builder.read(indicesIn,
                 Gpu::BufferUsageFlagBits::StorageBufferBit |
                     Gpu::BufferUsageFlagBits::ShaderDeviceAddressBit,
                 Sync::AccessType::ComputeShaderReadOther);
    builder.read(histogramsHandle,
                 Gpu::BufferUsageFlagBits::StorageBufferBit |
                     Gpu::BufferUsageFlagBits::ShaderDeviceAddressBit,
                 Sync::AccessType::ComputeShaderReadOther);
    auto [writtenKeys, keysInfo] =
        builder.write(keysOut,
                      Gpu::BufferUsageFlagBits::StorageBufferBit |
                          Gpu::BufferUsageFlagBits::ShaderDeviceAddressBit,
                      Sync::AccessType::ComputeShaderWrite);
    auto [writtenIndices, indicesInfo] =
        builder.write(indicesOut,
                      Gpu::BufferUsageFlagBits::StorageBufferBit |
                          Gpu::BufferUsageFlagBits::ShaderDeviceAddressBit,
                      Sync::AccessType::ComputeShaderWrite);
    (void)keysInfo;
    (void)indicesInfo;

    auto pass = builder.declareComputePass(ComputePassDeclaration{
        .name = fmt::format("PASS_RadixScatter_{}", bitOffset),
        .shader = scatterShader,
    });

    RenderInput renderApi =
        co_await builder.finishDeclaration(ScatterOutput{writtenKeys, writtenIndices});

    auto recorder = pass.begin(renderApi);
    recorder.bindShader(renderApi.ctx->shaders()[scatterShader].shaderHandle());

    struct RadixScatterPushConstants {
        uint32_t numInstances;
        uint32_t bitOffset;
        BufferDeviceAddress keys;
        BufferDeviceAddress indices;
        BufferDeviceAddress histograms;
        BufferDeviceAddress keysOut;
        BufferDeviceAddress indicesOut;
    } pc{instanceCount,
         bitOffset,
         renderApi.resources->deviceAddress(keysIn),
         renderApi.resources->deviceAddress(indicesIn),
         renderApi.resources->deviceAddress(histogramsHandle),
         renderApi.resources->deviceAddress(writtenKeys),
         renderApi.resources->deviceAddress(writtenIndices)};

    renderApi.bindingContext->push(pc);
    recorder.dispatchCompute({workgroups, 1, 1});
    pass.end(std::move(recorder));
}
} // namespace

RadixSorter::RadixSorter(Context &ctx)
    : ctx_{&ctx}
{
    histogramShader_ = createComputeShader(ctx, "shaders/RadixSortHistogram.comp.slang");
    scanShader_ = createComputeShader(ctx, "shaders/RadixSortScan.comp.slang");
    scatterShader_ = createComputeShader(ctx, "shaders/RadixSortScatter.comp.slang");
}

RadixSorter::~RadixSorter()
{
    if (!ctx_) return;
    auto &shaders = ctx_->shaders();
    shaders.release(histogramShader_);
    shaders.release(scanShader_);
    shaders.release(scatterShader_);
}

RadixSorter::SortOutput RadixSorter::sort(RenderTaskBuilder &builder,
                                          TransientBufferHandle predicateBuffer,
                                          uint32_t instanceCount)
{
    const Gpu::DeviceSize required = static_cast<Gpu::DeviceSize>(instanceCount) * sizeof(uint32_t);
    const uint32_t workgroups = divideRoundUp(instanceCount, kWorkgroupSize);
    const Gpu::DeviceSize histogramSize =
        static_cast<Gpu::DeviceSize>(workgroups) * 16u * sizeof(uint32_t);

    auto keysA = predicateBuffer;
    auto setupTask = radixSetupTask(builder.subtask("RadixSetup"), required, histogramSize);
    auto setup = setupTask.output();
    auto keysB = setup.keysB;
    auto indicesA = setup.indicesA;
    auto indicesB = setup.indicesB;
    auto histograms = setup.histograms;

    auto initIndicesTask =
        radixInitIndicesTask(builder.subtask("RadixInitIndices"), indicesA, instanceCount);

    auto keysIn = keysA;
    auto keysOut = keysB;
    auto indicesIn = initIndicesTask.output();
    auto indicesOut = indicesB;
    auto histogramsHandle = histograms;

    for (uint32_t bitOffset = 0; bitOffset < 32; bitOffset += 4) {
        auto histogramTask =
            radixHistogramTask(builder.subtask(fmt::format("RadixHistogram_{}", bitOffset)),
                               keysIn,
                               indicesIn,
                               histogramsHandle,
                               instanceCount,
                               workgroups,
                               bitOffset,
                               histogramShader_);

        auto scanTask = radixScanTask(builder.subtask(fmt::format("RadixScan_{}", bitOffset)),
                                      histogramTask.output(),
                                      workgroups,
                                      bitOffset,
                                      scanShader_);

        auto scatterTask =
            radixScatterTask(builder.subtask(fmt::format("RadixScatter_{}", bitOffset)),
                             keysIn,
                             indicesIn,
                             keysOut,
                             indicesOut,
                             scanTask.output(),
                             instanceCount,
                             workgroups,
                             bitOffset,
                             scatterShader_);

        auto scatterOut = scatterTask.output();
        keysOut = keysIn;
        indicesOut = indicesIn;
        keysIn = scatterOut.keys;
        indicesIn = scatterOut.indices;
        histogramsHandle = scanTask.output();
    }

    return SortOutput{
        .keys = keysIn,
        .indices = indicesIn,
    };
}

} // namespace Cory
