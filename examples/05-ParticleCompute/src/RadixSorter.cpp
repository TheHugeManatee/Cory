#include "RadixSorter.hpp"

#include <Cory/Base/Log.hpp>
#include <Cory/Base/ResourceLocator.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/DescriptorSets.hpp>
#include <Cory/Renderer/PipelineCache.hpp>
#include <Cory/Renderer/ShaderManager.hpp>

#include <KDGpu/bind_group_options.h>
#include <KDGpu/bind_group_pool_options.h>
#include <KDGpu/buffer_options.h>
#include <KDGpu/command_recorder.h>

#include <cstring>
#include <numeric>
#include <utility>

namespace Cory {
namespace {
constexpr uint32_t kWorkgroupSize = 256u;
constexpr Gpu::PushConstantRange kPushRange{
    .offset = 0,
    .size = sizeof(uint32_t) * 2,
    .shaderStages = Gpu::ShaderStageFlagBits::ComputeBit,
};

ShaderHandle createComputeShader(Context &ctx, std::string_view path)
{
    ShaderSource source{ResourceLocator::Locate(path)};
    return ctx.shaders().createShader(std::move(source), {kPushRange});
}
} // namespace

RadixSorter::RadixSorter(Context &ctx)
    : ctx_{&ctx}
{
    histogramShader_ = createComputeShader(ctx, "radix_sort_histogram.comp.slang");
    scanShader_ = createComputeShader(ctx, "radix_sort_scan.comp.slang");
    scatterShader_ = createComputeShader(ctx, "radix_sort_scatter.comp.slang");
}

RadixSorter::~RadixSorter()
{
    if (!ctx_) return;
    auto &shaders = ctx_->shaders();
    shaders.release(histogramShader_);
    shaders.release(scanShader_);
    shaders.release(scatterShader_);
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

void RadixSorter::ensurePipelineLayout()
{
    if (pipelineLayout_.isValid()) return;

    auto &cache = ctx_->pipelineCache();
    auto layouts = ctx_->descriptors().layouts();
    pipelineLayout_ = cache.queryLayout(Gpu::PipelineLayoutOptions{
        .label = "RadixSortLayout",
        .bindGroupLayouts = layouts,
        .pushConstantRanges = {kPushRange},
    });
}

void RadixSorter::ensurePipeline()
{
    if (computePipeline_.isValid()) return;

    ensurePipelineLayout();
    auto &cache = ctx_->pipelineCache();
    computePipeline_ = cache.queryComputePipeline(
        "RadixSortCompute",
        ComputePipelineDescriptor{.shader = histogramShader_, .pipelineLayout = pipelineLayout_});
    CO_CORE_ASSERT(computePipeline_.isValid(), "Failed to create radix sort compute pipeline");
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
Gpu::BindGroup &pushBindGroup(Context &ctx,
                              RadixSorter::ScratchBuffers &scratch,
                              std::string_view label,
                              std::vector<Gpu::BindGroupEntry> entries)
{
    auto bindGroup = ctx.device().createBindGroup(Gpu::BindGroupOptions{
        .label = std::string(label),
        .layout = ctx.descriptors().layouts()[0],
        .resources = std::move(entries),
        .bindGroupPool = scratch.bindGroupPool,
    });
    scratch.bindGroups.push_back(std::move(bindGroup));
    return scratch.bindGroups.back();
}
} // namespace

Gpu::Buffer &RadixSorter::sort(Gpu::CommandRecorder &cmd,
                               DescriptorSets &descriptors,
                               ScratchBuffers &scratch,
                               const Gpu::Buffer &predicateBuffer,
                               uint32_t instanceCount,
                               Gpu::Buffer &outputIndices,
                               uint32_t frameInFlightIndex)
{
    ensurePipeline();
    ensureScratch(scratch, instanceCount, frameInFlightIndex);
    scratch.bindGroups.clear();

    auto barrier = [&](const Gpu::Buffer &buf, Gpu::AccessFlags srcMask, Gpu::AccessFlags dstMask) {
        cmd.bufferMemoryBarrier(Gpu::BufferMemoryBarrierOptions{
            .srcStages = Gpu::PipelineStageFlagBit::ComputeShaderBit,
            .srcMask = srcMask,
            .dstStages = Gpu::PipelineStageFlagBit::ComputeShaderBit,
            .dstMask = dstMask,
            .buffer = buf.handle(),
        });
    };

    const uint32_t numWorkgroups = scratch.workgroups;

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
        dispatchHistogram(
            cmd, descriptors, scratch, *keysIn, instanceCount, bitOffset, frameInFlightIndex);
        barrier(scratch.histograms,
                Gpu::AccessFlagBit::ShaderStorageWriteBit,
                Gpu::AccessFlags(Gpu::AccessFlagBit::ShaderStorageReadBit |
                                 Gpu::AccessFlagBit::ShaderStorageWriteBit));

        // Scan
        dispatchScan(cmd, descriptors, scratch, frameInFlightIndex);
        barrier(scratch.histograms,
                Gpu::AccessFlagBit::ShaderStorageWriteBit,
                Gpu::AccessFlagBit::ShaderStorageReadBit);

        // Scatter
        dispatchScatter(cmd,
                        descriptors,
                        scratch,
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

void RadixSorter::dispatchHistogram(Gpu::CommandRecorder &cmd,
                                    DescriptorSets &descriptors,
                                    ScratchBuffers &scratch,
                                    const Gpu::Buffer &keys,
                                    uint32_t instanceCount,
                                    uint32_t bitOffset,
                                    uint32_t frameInFlightIndex)
{
    ensurePipeline();
    ensureScratch(scratch, instanceCount, frameInFlightIndex);
    (void)descriptors;

    auto &shaders = ctx_->shaders();
    auto pass = cmd.beginComputePass({});
    pass.setPipeline(computePipeline_);
    pass.bindShader(shaders[histogramShader_].shaderHandle());

    auto &bindGroup = pushBindGroup(*ctx_,
                                    scratch,
                                    "RadixHistogram",
                                    {
                                        Gpu::BindGroupEntry{
                                            .binding = 2,
                                            .resource = Gpu::StorageBufferBinding{
                                                .buffer = keys.handle(),
                                                .offset = 0,
                                                .size = Gpu::StorageBufferBinding::WholeSize,
                                            },
                                            .arrayElement = 0,
                                        },
                                        Gpu::BindGroupEntry{
                                            .binding = 3,
                                            .resource = Gpu::StorageBufferBinding{
                                                .buffer = scratch.histograms.handle(),
                                                .offset = 0,
                                                .size = Gpu::StorageBufferBinding::WholeSize,
                                            },
                                            .arrayElement = 0,
                                        },
                                    });
    pass.setBindGroup(0, bindGroup);
    struct {
        uint32_t numInstances;
        uint32_t bitOffset;
    } pc{instanceCount, bitOffset};
    pass.pushConstant(kPushRange, &pc);
    pass.dispatchCompute({scratch.workgroups, 1, 1});
    pass.end();
}

void RadixSorter::dispatchScan(Gpu::CommandRecorder &cmd,
                               DescriptorSets &descriptors,
                               ScratchBuffers &scratch,
                               uint32_t frameInFlightIndex)
{
    ensurePipeline();
    (void)descriptors;
    auto &shaders = ctx_->shaders();
    auto pass = cmd.beginComputePass({});
    pass.setPipeline(computePipeline_);
    pass.bindShader(shaders[scanShader_].shaderHandle());
    auto &bindGroup = pushBindGroup(*ctx_,
                                    scratch,
                                    "RadixScan",
                                    {
                                        Gpu::BindGroupEntry{
                                            .binding = 2,
                                            .resource = Gpu::StorageBufferBinding{
                                                .buffer = scratch.histograms.handle(),
                                                .offset = 0,
                                                .size = Gpu::StorageBufferBinding::WholeSize,
                                            },
                                            .arrayElement = 0,
                                        },
                                    });
    pass.setBindGroup(0, bindGroup);
    struct {
        uint32_t numWorkgroups;
        uint32_t pad;
    } pc{scratch.workgroups, 0u};
    pass.pushConstant(kPushRange, &pc);
    pass.dispatchCompute({1, 1, 1});
    pass.end();
}

void RadixSorter::dispatchScatter(Gpu::CommandRecorder &cmd,
                                  DescriptorSets &descriptors,
                                  ScratchBuffers &scratch,
                                  const Gpu::Buffer &keysIn,
                                  const Gpu::Buffer &indicesIn,
                                  Gpu::Buffer &keysOut,
                                  Gpu::Buffer &indicesOut,
                                  uint32_t instanceCount,
                                  uint32_t bitOffset,
                                  uint32_t frameInFlightIndex)
{
    ensurePipeline();
    ensureScratch(scratch, instanceCount, frameInFlightIndex);
    (void)descriptors;

    auto &shaders = ctx_->shaders();
    auto pass = cmd.beginComputePass({});
    pass.setPipeline(computePipeline_);
    pass.bindShader(shaders[scatterShader_].shaderHandle());
    auto &bindGroup =
        pushBindGroup(*ctx_,
                      scratch,
                      "RadixScatter",
                      {
                          Gpu::BindGroupEntry{
                              .binding = 2,
                              .resource = Gpu::StorageBufferBinding{
                                  .buffer = keysIn.handle(),
                                  .offset = 0,
                                  .size = Gpu::StorageBufferBinding::WholeSize,
                              },
                              .arrayElement = 0,
                          },
                          Gpu::BindGroupEntry{
                              .binding = 3,
                              .resource = Gpu::StorageBufferBinding{
                                  .buffer = indicesIn.handle(),
                                  .offset = 0,
                                  .size = Gpu::StorageBufferBinding::WholeSize,
                              },
                              .arrayElement = 0,
                          },
                          Gpu::BindGroupEntry{
                              .binding = 4,
                              .resource = Gpu::StorageBufferBinding{
                                  .buffer = keysOut.handle(),
                                  .offset = 0,
                                  .size = Gpu::StorageBufferBinding::WholeSize,
                              },
                              .arrayElement = 0,
                          },
                          Gpu::BindGroupEntry{
                              .binding = 5,
                              .resource = Gpu::StorageBufferBinding{
                                  .buffer = indicesOut.handle(),
                                  .offset = 0,
                                  .size = Gpu::StorageBufferBinding::WholeSize,
                              },
                              .arrayElement = 0,
                          },
                          Gpu::BindGroupEntry{
                              .binding = 6,
                              .resource = Gpu::StorageBufferBinding{
                                  .buffer = scratch.histograms.handle(),
                                  .offset = 0,
                                  .size = Gpu::StorageBufferBinding::WholeSize,
                              },
                              .arrayElement = 0,
                          },
                      });
    pass.setBindGroup(0, bindGroup);
    struct {
        uint32_t numInstances;
        uint32_t bitOffset;
    } pc{instanceCount, bitOffset};
    pass.pushConstant(kPushRange, &pc);
    pass.dispatchCompute({scratch.workgroups, 1, 1});
    pass.end();
}
} // namespace Cory
