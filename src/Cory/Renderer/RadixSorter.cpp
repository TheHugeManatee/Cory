#include "RadixSorter.hpp"

#include <Cory/Base/Log.hpp>
#include <Cory/Base/ResourceLocator.hpp>
#include <Cory/Framegraph/FramegraphResourceManager.hpp>
#include <Cory/Framegraph/RenderTaskBuilder.hpp>
#include <Cory/Framegraph/RenderTaskDeclaration.hpp>
#include <Cory/Framegraph/ShaderBindingContext.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/DescriptorSets.hpp>
#include <Cory/Renderer/ShaderManager.hpp>

#include <KDGpu/bind_group_options.h>
#include <KDGpu/bind_group_pool_options.h>
#include <KDGpu/buffer_options.h>
#include <KDGpu/command_recorder.h>

#include <KDGpu/vulkan/vulkan_buffer.h>
#include <array>
#include <cstring>
#include <numeric>
#include <utility>
#include <variant>

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

RenderTaskDeclaration<RadixSortBuffers> radixSetupTask(RenderTaskBuilder builder,
                                                       Gpu::DeviceSize required,
                                                       Gpu::DeviceSize histogramSize)
{
    auto keysB = builder.create("BUF_RadixSortKeysB",
                                required,
                                Gpu::BufferUsageFlagBits::StorageBufferBit,
                                Sync::AccessType::ComputeShaderWrite);
    auto indicesA = builder.create("BUF_RadixSortIndicesA",
                                   required,
                                   Gpu::BufferUsageFlagBits::StorageBufferBit,
                                   Sync::AccessType::HostWrite,
                                   Gpu::MemoryUsage::CpuToGpu);
    auto indicesB = builder.create("BUF_RadixSortIndicesB",
                                   required,
                                   Gpu::BufferUsageFlagBits::StorageBufferBit,
                                   Sync::AccessType::ComputeShaderWrite);
    auto histograms = builder.create("BUF_RadixSortHistograms",
                                     histogramSize,
                                     Gpu::BufferUsageFlagBits::StorageBufferBit,
                                     Sync::AccessType::ComputeShaderWrite);
    [[maybe_unused]] RenderInput render =
        co_await builder.finishDeclaration(RadixSortBuffers{
            .keysB = keysB,
            .indicesA = indicesA,
            .indicesB = indicesB,
            .histograms = histograms,
        });
}

RenderTaskDeclaration<TransientBufferHandle>
radixInitIndicesTask(RenderTaskBuilder builder,
                     TransientBufferHandle indicesHandle,
                     uint32_t instanceCount)
{
    auto [writtenIndices, info] = builder.write(indicesHandle, Sync::AccessType::HostWrite);
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
    builder.read(keysHandle, Sync::AccessType::ComputeShaderReadOther);
    // Ensure each radix step depends on the previous scatter's outputs.
    builder.read(indicesHandle, Sync::AccessType::ComputeShaderReadOther);
    auto [writtenHistograms, info] =
        builder.write(histogramsHandle, Sync::AccessType::ComputeShaderWrite);
    (void)info;

    auto pass = builder.declareComputePass(ComputePassDeclaration{
        .name = fmt::format("PASS_RadixHistogram_{}", bitOffset),
        .shader = histogramShader,
    });

    RenderInput renderApi = co_await builder.finishDeclaration(writtenHistograms);

    auto recorder = pass.begin(renderApi);
    recorder.bindShader(renderApi.ctx->shaders()[histogramShader].shaderHandle());

    const auto keysIndex =
        renderApi.bindingContext->bindBuffer(keysHandle, BufferBindPoint::StorageBufferReadOnly);
    const auto histogramIndex = renderApi.bindingContext->bindBuffer(
        writtenHistograms, BufferBindPoint::StorageBufferReadWrite);

    renderApi.bindingContext->flush();

    struct RadixHistogramPushConstants {
        uint32_t numInstances;
        uint32_t bitOffset;
        uint32_t keysIndex;
        uint32_t histogramIndex;
    } pc{instanceCount, bitOffset, keysIndex, histogramIndex};

    renderApi.bindingContext->push(pc);
    recorder.dispatchCompute({workgroups, 1, 1});
    pass.end(std::move(recorder));
}

RenderTaskDeclaration<TransientBufferHandle>
radixScanTask(RenderTaskBuilder builder,
              TransientBufferHandle histogramsHandle,
              uint32_t workgroups,
              uint32_t bitOffset,
              ShaderHandle scanShader)
{
    auto [writtenHistograms, info] =
        builder.readWrite(histogramsHandle, Sync::AccessType::ComputeShaderWrite);
    (void)info;

    auto pass = builder.declareComputePass(ComputePassDeclaration{
        .name = fmt::format("PASS_RadixScan_{}", bitOffset),
        .shader = scanShader,
    });

    RenderInput renderApi = co_await builder.finishDeclaration(writtenHistograms);

    auto recorder = pass.begin(renderApi);
    recorder.bindShader(renderApi.ctx->shaders()[scanShader].shaderHandle());

    const auto histogramIndex = renderApi.bindingContext->bindBuffer(
        writtenHistograms, BufferBindPoint::StorageBufferReadWrite);

    renderApi.bindingContext->flush();

    struct RadixScanPushConstants {
        uint32_t numWorkgroups;
        uint32_t histogramIndex;
    } pc{workgroups, histogramIndex};

    renderApi.bindingContext->push(pc);
    recorder.dispatchCompute({1, 1, 1});
    pass.end(std::move(recorder));
}

RenderTaskDeclaration<ScatterOutput>
radixScatterTask(RenderTaskBuilder builder,
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
    builder.read(keysIn, Sync::AccessType::ComputeShaderReadOther);
    builder.read(indicesIn, Sync::AccessType::ComputeShaderReadOther);
    builder.read(histogramsHandle, Sync::AccessType::ComputeShaderReadOther);
    auto [writtenKeys, keysInfo] =
        builder.write(keysOut, Sync::AccessType::ComputeShaderWrite);
    auto [writtenIndices, indicesInfo] =
        builder.write(indicesOut, Sync::AccessType::ComputeShaderWrite);
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

    const auto keysIndex =
        renderApi.bindingContext->bindBuffer(keysIn, BufferBindPoint::StorageBufferReadOnly);
    const auto indicesIndex =
        renderApi.bindingContext->bindBuffer(indicesIn, BufferBindPoint::StorageBufferReadOnly);
    const auto histogramIndex = renderApi.bindingContext->bindBuffer(
        histogramsHandle, BufferBindPoint::StorageBufferReadOnly);
    const auto keysOutIndex = renderApi.bindingContext->bindBuffer(
        writtenKeys, BufferBindPoint::StorageBufferReadWrite);
    const auto indicesOutIndex = renderApi.bindingContext->bindBuffer(
        writtenIndices, BufferBindPoint::StorageBufferReadWrite);

    renderApi.bindingContext->flush();

    struct RadixScatterPushConstants {
        uint32_t numInstances;
        uint32_t bitOffset;
        uint32_t keysIndex;
        uint32_t indicesIndex;
        uint32_t histogramIndex;
        uint32_t keysOutIndex;
        uint32_t indicesOutIndex;
    } pc{instanceCount,
         bitOffset,
         keysIndex,
         indicesIndex,
         histogramIndex,
         keysOutIndex,
         indicesOutIndex};

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

RadixSorter::Passes RadixSorter::declarePasses(FramegraphResourceManager &resources)
{
    return Passes{
        .histogram = TransientComputePass{*ctx_,
                                          resources,
                                          ComputePassDeclaration{
                                              .name = "PASS_RadixHistogram",
                                              .shader = histogramShader_,
                                          }},
        .scan = TransientComputePass{*ctx_,
                                     resources,
                                     ComputePassDeclaration{
                                         .name = "PASS_RadixScan",
                                         .shader = scanShader_,
                                     }},
        .scatter = TransientComputePass{*ctx_,
                                        resources,
                                        ComputePassDeclaration{
                                            .name = "PASS_RadixScatter",
                                            .shader = scatterShader_,
                                        }},
    };
}

RadixSorter::ScratchBuffers &RadixSorter::scratchForFrame(uint32_t frameIndex,
                                                          uint32_t instanceCount)
{
    if (perFrameScratch_.size() <= frameIndex) {
        perFrameScratch_.resize(frameIndex + 1);
    }
    ensureScratch(perFrameScratch_[frameIndex], instanceCount, frameIndex);
    return perFrameScratch_[frameIndex];
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
    auto setupTask =
        radixSetupTask(builder.subtask("RadixSetup"), required, histogramSize);
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

        auto scatterTask = radixScatterTask(builder.subtask(fmt::format("RadixScatter_{}", bitOffset)),
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

void RadixSorter::ensureScratch(ScratchBuffers &scratch,
                                uint32_t instanceCount,
                                uint32_t scratchIndex)
{
    const Gpu::DeviceSize required = static_cast<Gpu::DeviceSize>(instanceCount) * sizeof(uint32_t);
    const uint32_t workgroups = divideRoundUp(instanceCount, kWorkgroupSize);

    if (scratch.capacity >= required) {
        scratch.workgroups = workgroups;
        if (!scratch.bindGroupPool.isValid()) {
            scratch.bindGroupPool = ctx_->device().createBindGroupPool(Gpu::BindGroupPoolOptions{
                .label = fmt::format("RadixSort BindGroupPool {}", scratchIndex),
                .uniformBufferCount = 16,
                .dynamicUniformBufferCount = 0,
                .storageBufferCount = 1024,
                .textureSamplerCount = 0,
                .textureCount = 0,
                .samplerCount = 0,
                .imageCount = 0,
                .inputAttachmentCount = 0,
                .accelerationStructureCount = 0,
                .maxBindGroupCount = 1024,
                .flags = Gpu::BindGroupPoolFlagBits::CreateFreeBindGroups |
                         Gpu::BindGroupPoolFlagBits::UpdateAfterBind,
            });
        }
        return;
    }

    scratch.capacity = required;
    scratch.workgroups = workgroups;

    auto make_buffer = [&](std::string_view label) {
        return ctx_->device().createBuffer({
            .label = label,
            .size = required,
            .usage = Gpu::BufferUsageFlagBits::StorageBufferBit |
                     Gpu::BufferUsageFlagBits::TransferSrcBit |
                     Gpu::BufferUsageFlagBits::TransferDstBit,
            .memoryUsage = Gpu::MemoryUsage::CpuToGpu,
        });
    };
    scratch.keysA = make_buffer(fmt::format("Keys A {}", scratchIndex));
    scratch.keysB = make_buffer(fmt::format("Keys B {}", scratchIndex));
    scratch.indicesA = make_buffer(fmt::format("Indices A {}", scratchIndex));
    scratch.indicesB = make_buffer(fmt::format("Indices B {}", scratchIndex));

    scratch.histograms = ctx_->device().createBuffer({
        .label = fmt::format("RadixHist {}", scratchIndex),
        .size = static_cast<Gpu::DeviceSize>(workgroups) * 16 * sizeof(uint32_t),
        .usage = Gpu::BufferUsageFlagBits::StorageBufferBit |
                 Gpu::BufferUsageFlagBits::TransferSrcBit |
                 Gpu::BufferUsageFlagBits::TransferDstBit,
        .memoryUsage = Gpu::MemoryUsage::CpuToGpu,
    });

    scratch.bindGroupPool = ctx_->device().createBindGroupPool(Gpu::BindGroupPoolOptions{
        .label = fmt::format("RadixSort BindGroupPool {}", scratchIndex),
        .uniformBufferCount = 32,
        .dynamicUniformBufferCount = 0,
        .storageBufferCount = 128,
        .textureSamplerCount = 8,
        .textureCount = 8,
        .samplerCount = 0,
        .imageCount = 0,
        .inputAttachmentCount = 0,
        .accelerationStructureCount = 0,
        .maxBindGroupCount = 128,
        .flags = Gpu::BindGroupPoolFlagBits::CreateFreeBindGroups |
                 Gpu::BindGroupPoolFlagBits::UpdateAfterBind,
    });
}

namespace {
constexpr uint32_t kBufferSetIndex = static_cast<uint32_t>(DescriptorSetType::BindlessBuffers);
constexpr BufferHeapIndex kKeysIndex = 0;
constexpr BufferHeapIndex kIndicesIndex = 1;
constexpr BufferHeapIndex kHistogramsIndex = 2;
constexpr BufferHeapIndex kWriteBufferIndex = 0;
constexpr BufferHeapIndex kWriteIndicesIndex = 1;

Gpu::BindGroup &pushBindGroup(Context &ctx,
                              RadixSorter::ScratchBuffers &scratch,
                              std::string_view label,
                              std::vector<Gpu::BindGroupEntry> entries)
{
    auto bindGroup = ctx.device().createBindGroup(Gpu::BindGroupOptions{
        .label = std::string(label),
        .layout = ctx.descriptors().layouts()[kBufferSetIndex],
        .resources = std::move(entries),
        .bindGroupPool = scratch.bindGroupPool,
    });
    scratch.bindGroups.push_back(std::move(bindGroup));
    return scratch.bindGroups.back();
}
} // namespace

Gpu::Buffer &RadixSorter::sortImmediate(Gpu::CommandRecorder &cmd,
                                        ScratchBuffers &scratch,
                                        Passes &passes,
                                        const Gpu::Buffer &predicateBuffer,
                                        uint32_t instanceCount,
                                        Gpu::Buffer &outputIndices,
                                        uint32_t frameInFlightIndex)
{
    ensureScratch(scratch, instanceCount, frameInFlightIndex);
    scratch.bindGroups.clear();

    FramegraphResourceManager resources{*ctx_};
    ShaderBindingContext bindingContext{
        ctx_->device(), resources, ctx_->descriptors(), frameInFlightIndex, 2 * 1024 * 1024};
    RenderInput renderApi{
        .ctx = ctx_,
        .frameCtx = nullptr,
        .resources = &resources,
        .bindingContext = &bindingContext,
        .cmd = &cmd,
    };

    auto barrier = [&](const Gpu::Buffer &buf, Gpu::AccessFlags srcMask, Gpu::AccessFlags dstMask) {
        cmd.bufferMemoryBarrier(Gpu::BufferMemoryBarrierOptions{
            .srcStages = Gpu::PipelineStageFlagBit::ComputeShaderBit,
            .srcMask = srcMask,
            .dstStages = Gpu::PipelineStageFlagBit::ComputeShaderBit,
            .dstMask = dstMask,
            .buffer = buf.handle(),
        });
    };

    if (predicateBuffer.handle() != scratch.keysA.handle()) {
        cmd.bufferMemoryBarrier(Gpu::BufferMemoryBarrierOptions{
            .srcStages = Gpu::PipelineStageFlagBit::HostBit,
            .srcMask = Gpu::AccessFlagBit::HostWriteBit,
            .dstStages = Gpu::PipelineStageFlagBit::TransferBit,
            .dstMask = Gpu::AccessFlagBit::TransferReadBit,
            .buffer = predicateBuffer.handle(),
        });
        cmd.copyBuffer(Gpu::BufferCopy{
            .src = predicateBuffer.handle(),
            .dst = scratch.keysA.handle(),
            .byteSize = static_cast<size_t>(instanceCount) * sizeof(uint32_t),
        });
        cmd.bufferMemoryBarrier(Gpu::BufferMemoryBarrierOptions{
            .srcStages = Gpu::PipelineStageFlagBit::TransferBit,
            .srcMask = Gpu::AccessFlagBit::TransferWriteBit,
            .dstStages = Gpu::PipelineStageFlagBit::ComputeShaderBit,
            .dstMask = Gpu::AccessFlagBit::ShaderStorageReadBit,
            .buffer = scratch.keysA.handle(),
        });
    }
    else {
        barrier(scratch.keysA,
                Gpu::AccessFlagBit::ShaderStorageWriteBit,
                Gpu::AccessFlagBit::ShaderStorageReadBit);
    }

    // Initialize indicesA on the CPU to avoid vkCmdUpdateBuffer size limits.
    {
        std::vector<uint32_t> initIndices(instanceCount);
        std::iota(initIndices.begin(), initIndices.end(), 0u);
        auto *mapped = static_cast<uint32_t *>(scratch.indicesA.map());
        std::memcpy(mapped, initIndices.data(), initIndices.size() * sizeof(uint32_t));
        scratch.indicesA.flush();
        scratch.indicesA.unmap();
        cmd.bufferMemoryBarrier(Gpu::BufferMemoryBarrierOptions{
            .srcStages = Gpu::PipelineStageFlagBit::HostBit,
            .srcMask = Gpu::AccessFlagBit::HostWriteBit,
            .dstStages = Gpu::PipelineStageFlagBit::ComputeShaderBit,
            .dstMask = Gpu::AccessFlagBit::ShaderStorageReadBit,
            .buffer = scratch.indicesA.handle(),
        });
    }

    Gpu::Buffer *keysIn = &scratch.keysA;
    Gpu::Buffer *keysOut = &scratch.keysB;
    Gpu::Buffer *indicesIn = &scratch.indicesA;
    Gpu::Buffer *indicesOut = &scratch.indicesB;

    for (uint32_t bitOffset = 0; bitOffset < 32; bitOffset += 4) {
        // Histogram
        dispatchHistogram(renderApi,
                          scratch,
                          passes.histogram,
                          *keysIn,
                          instanceCount,
                          bitOffset,
                          frameInFlightIndex);
        barrier(scratch.histograms,
                Gpu::AccessFlagBit::ShaderStorageWriteBit,
                Gpu::AccessFlags(Gpu::AccessFlagBit::ShaderStorageReadBit |
                                 Gpu::AccessFlagBit::ShaderStorageWriteBit));

        // Scan
        dispatchScan(renderApi, scratch, passes.scan, frameInFlightIndex);
        barrier(scratch.histograms,
                Gpu::AccessFlagBit::ShaderStorageWriteBit,
                Gpu::AccessFlagBit::ShaderStorageReadBit);

        // Scatter
        dispatchScatter(renderApi,
                        scratch,
                        passes.scatter,
                        *keysIn,
                        *indicesIn,
                        *keysOut,
                        *indicesOut,
                        instanceCount,
                        bitOffset,
                        frameInFlightIndex);
        barrier(*keysOut,
                Gpu::AccessFlagBit::ShaderStorageWriteBit,
                Gpu::AccessFlagBit::ShaderStorageReadBit);
        barrier(*indicesOut,
                Gpu::AccessFlagBit::ShaderStorageWriteBit,
                Gpu::AccessFlagBit::ShaderStorageReadBit);
        barrier(scratch.histograms,
                Gpu::AccessFlagBit::ShaderStorageReadBit,
                Gpu::AccessFlagBit::ShaderStorageWriteBit);

        std::swap(keysIn, keysOut);
        std::swap(indicesIn, indicesOut);
    }

    if (indicesIn->handle() != outputIndices.handle()) {
        cmd.bufferMemoryBarrier(Gpu::BufferMemoryBarrierOptions{
            .srcStages = Gpu::PipelineStageFlagBit::ComputeShaderBit,
            .srcMask = Gpu::AccessFlagBit::ShaderStorageWriteBit,
            .dstStages = Gpu::PipelineStageFlagBit::TransferBit,
            .dstMask = Gpu::AccessFlagBit::TransferReadBit,
            .buffer = indicesIn->handle(),
        });
        cmd.copyBuffer(Gpu::BufferCopy{
            .src = indicesIn->handle(),
            .dst = outputIndices.handle(),
            .byteSize = static_cast<size_t>(instanceCount) * sizeof(uint32_t),
        });
    }

    return outputIndices;
}

void RadixSorter::dispatchHistogram(RenderInput &renderApi,
                                    ScratchBuffers &scratch,
                                    TransientComputePass &computePass,
                                    const Gpu::Buffer &keys,
                                    uint32_t instanceCount,
                                    uint32_t bitOffset,
                                    uint32_t frameInFlightIndex)
{
    ensureScratch(scratch, instanceCount, frameInFlightIndex);

    auto &shaders = ctx_->shaders();
    auto pass = computePass.begin(renderApi);
    pass.bindShader(shaders[histogramShader_].shaderHandle());

    auto &bindGroup = pushBindGroup(*ctx_,
                                    scratch,
                                    "RadixHistogram",
                                    {
                                        Gpu::BindGroupEntry{
                                            .binding = BufferBindPoint::StorageBufferReadOnly,
                                            .resource =
                                                Gpu::StorageBufferBinding{
                                                    .buffer = keys.handle(),
                                                    .offset = 0,
                                                    .size = Gpu::StorageBufferBinding::WholeSize,
                                                },
                                            .arrayElement = kKeysIndex,
                                        },
                                        Gpu::BindGroupEntry{
                                            .binding = BufferBindPoint::StorageBufferReadWrite,
                                            .resource =
                                                Gpu::StorageBufferBinding{
                                                    .buffer = scratch.histograms.handle(),
                                                    .offset = 0,
                                                    .size = Gpu::StorageBufferBinding::WholeSize,
                                                },
                                            .arrayElement = kWriteBufferIndex,
                                        },
                                    });
    pass.setBindGroup(kBufferSetIndex, bindGroup);
    struct {
        uint32_t numInstances;
        uint32_t bitOffset;
        uint32_t keysIndex;
        uint32_t histogramIndex;
    } pc{instanceCount, bitOffset, kKeysIndex, kWriteBufferIndex};
    pass.pushConstant(Gpu::PushConstantRange{.offset = 0,
                                             .size = sizeof(pc),
                                             .shaderStages = Gpu::ShaderStageFlagBits::ComputeBit},
                      &pc);
    pass.dispatchCompute({scratch.workgroups, 1, 1});
    computePass.end(std::move(pass));
}

void RadixSorter::dispatchScan(RenderInput &renderApi,
                               ScratchBuffers &scratch,
                               TransientComputePass &computePass,
                               uint32_t frameInFlightIndex)
{
    auto &shaders = ctx_->shaders();
    auto pass = computePass.begin(renderApi);
    pass.bindShader(shaders[scanShader_].shaderHandle());
    auto &bindGroup = pushBindGroup(
        *ctx_,
        scratch,
        "RadixScan",
        {
            Gpu::BindGroupEntry{
                .binding = static_cast<uint32_t>(BufferBindPoint::StorageBufferReadWrite),
                .resource =
                    Gpu::StorageBufferBinding{
                        .buffer = scratch.histograms.handle(),
                        .offset = 0,
                        .size = Gpu::StorageBufferBinding::WholeSize,
                    },
                .arrayElement = kWriteBufferIndex,
            },
        });
    pass.setBindGroup(kBufferSetIndex, bindGroup);
    struct {
        uint32_t numWorkgroups;
        uint32_t histogramIndex;
    } pc{scratch.workgroups, kWriteBufferIndex};
    pass.pushConstant(Gpu::PushConstantRange{.offset = 0,
                                             .size = sizeof(pc),
                                             .shaderStages = Gpu::ShaderStageFlagBits::ComputeBit},
                      &pc);
    pass.dispatchCompute({1, 1, 1});
    computePass.end(std::move(pass));
}

void RadixSorter::dispatchScatter(RenderInput &renderApi,
                                  ScratchBuffers &scratch,
                                  TransientComputePass &computePass,
                                  const Gpu::Buffer &keysIn,
                                  const Gpu::Buffer &indicesIn,
                                  Gpu::Buffer &keysOut,
                                  Gpu::Buffer &indicesOut,
                                  uint32_t instanceCount,
                                  uint32_t bitOffset,
                                  uint32_t frameInFlightIndex)
{
    ensureScratch(scratch, instanceCount, frameInFlightIndex);

    auto &shaders = ctx_->shaders();
    auto pass = computePass.begin(renderApi);
    pass.bindShader(shaders[scatterShader_].shaderHandle());
    auto &bindGroup = pushBindGroup(*ctx_,
                                    scratch,
                                    "RadixScatter",
                                    {
                                        Gpu::BindGroupEntry{
                                            .binding = BufferBindPoint::StorageBufferReadOnly,
                                            .resource =
                                                Gpu::StorageBufferBinding{
                                                    .buffer = keysIn.handle(),
                                                    .offset = 0,
                                                    .size = Gpu::StorageBufferBinding::WholeSize,
                                                },
                                            .arrayElement = kKeysIndex,
                                        },
                                        Gpu::BindGroupEntry{
                                            .binding = BufferBindPoint::StorageBufferReadOnly,
                                            .resource =
                                                Gpu::StorageBufferBinding{
                                                    .buffer = indicesIn.handle(),
                                                    .offset = 0,
                                                    .size = Gpu::StorageBufferBinding::WholeSize,
                                                },
                                            .arrayElement = kIndicesIndex,
                                        },
                                        Gpu::BindGroupEntry{
                                            .binding = BufferBindPoint::StorageBufferReadWrite,
                                            .resource =
                                                Gpu::StorageBufferBinding{
                                                    .buffer = keysOut.handle(),
                                                    .offset = 0,
                                                    .size = Gpu::StorageBufferBinding::WholeSize,
                                                },
                                            .arrayElement = kWriteBufferIndex,
                                        },
                                        Gpu::BindGroupEntry{
                                            .binding = BufferBindPoint::StorageBufferReadWrite,
                                            .resource =
                                                Gpu::StorageBufferBinding{
                                                    .buffer = indicesOut.handle(),
                                                    .offset = 0,
                                                    .size = Gpu::StorageBufferBinding::WholeSize,
                                                },
                                            .arrayElement = kWriteIndicesIndex,
                                        },
                                        Gpu::BindGroupEntry{
                                            .binding = BufferBindPoint::StorageBufferReadOnly,
                                            .resource =
                                                Gpu::StorageBufferBinding{
                                                    .buffer = scratch.histograms.handle(),
                                                    .offset = 0,
                                                    .size = Gpu::StorageBufferBinding::WholeSize,
                                                },
                                            .arrayElement = kHistogramsIndex,
                                        },
                                    });
    pass.setBindGroup(kBufferSetIndex, bindGroup);
    struct {
        uint32_t numInstances;
        uint32_t bitOffset;
        uint32_t keysIndex;
        uint32_t indicesIndex;
        uint32_t histogramIndex;
        uint32_t keysOutIndex;
        uint32_t indicesOutIndex;
    } pc{instanceCount,
         bitOffset,
         kKeysIndex,
         kIndicesIndex,
         kHistogramsIndex,
         kWriteBufferIndex,
         kWriteIndicesIndex};
    pass.pushConstant(Gpu::PushConstantRange{.offset = 0,
                                             .size = sizeof(pc),
                                             .shaderStages = Gpu::ShaderStageFlagBits::ComputeBit},
                      &pc);
    pass.dispatchCompute({scratch.workgroups, 1, 1});
    computePass.end(std::move(pass));
}
} // namespace Cory
