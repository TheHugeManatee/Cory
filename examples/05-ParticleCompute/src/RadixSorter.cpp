#include "RadixSorter.hpp"

#include <Cory/Base/Log.hpp>
#include <Cory/Base/ResourceLocator.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/DescriptorSets.hpp>
#include <Cory/Renderer/PipelineCache.hpp>
#include <Cory/Renderer/ShaderManager.hpp>

#include <KDGpu/buffer_options.h>
#include <KDGpu/command_recorder.h>
#include <KDGpu/vulkan/vulkan_compute_pass_command_recorder.h>
#include <KDGpu/vulkan/vulkan_resource_manager.h>

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
    ensureScratch(perFrameScratch_[frameIndex], instanceCount);
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

void RadixSorter::ensureScratch(ScratchBuffers &scratch, uint32_t instanceCount)
{
    const Gpu::DeviceSize required = static_cast<Gpu::DeviceSize>(instanceCount) * sizeof(uint32_t);
    const uint32_t workgroups = (instanceCount + kWorkgroupSize - 1) / kWorkgroupSize;

    if (scratch.capacity >= required) {
        scratch.workgroups = workgroups;
        return;
    }

    scratch.capacity = required;
    scratch.workgroups = workgroups;

    Gpu::BufferOptions opts{
        .label = "RadixSortBuf",
        .size = required,
        .usage = Gpu::BufferUsageFlagBits::StorageBufferBit |
                 Gpu::BufferUsageFlagBits::TransferSrcBit |
                 Gpu::BufferUsageFlagBits::TransferDstBit,
        .memoryUsage = Gpu::MemoryUsage::CpuToGpu,
    };
    scratch.keysA = ctx_->device().createBuffer(opts);
    scratch.keysB = ctx_->device().createBuffer(opts);
    scratch.indicesA = ctx_->device().createBuffer(opts);
    scratch.indicesB = ctx_->device().createBuffer(opts);

    opts.label = "RadixHist";
    opts.size = static_cast<Gpu::DeviceSize>(workgroups) * 16 * sizeof(uint32_t);
    scratch.histograms = ctx_->device().createBuffer(opts);
}

Gpu::Buffer &RadixSorter::sort(Gpu::CommandRecorder &cmd,
                               DescriptorSets &descriptors,
                               ScratchBuffers &scratch,
                               const Gpu::Buffer &predicateBuffer,
                               uint32_t instanceCount,
                               Gpu::Buffer &outputIndices,
                               uint32_t descriptorSetIndex)
{
    ensurePipeline();
    ensureScratch(scratch, instanceCount);

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

    // Initialize indicesA
    {
        auto *mapped = static_cast<uint32_t *>(scratch.indicesA.map());
        std::iota(mapped, mapped + instanceCount, 0u);
        scratch.indicesA.unmap();
    }

    Gpu::Buffer *keysIn = &scratch.keysA;
    Gpu::Buffer *keysOut = &scratch.keysB;
    Gpu::Buffer *indicesIn = &scratch.indicesA;
    Gpu::Buffer *indicesOut = &scratch.indicesB;

    auto &shaders = ctx_->shaders();
    auto pass = cmd.beginComputePass({});
    pass.setPipeline(computePipeline_);

    for (uint32_t bitOffset = 0; bitOffset < 32; bitOffset += 4) {
        // Histogram
        {
            pass.bindShader(shaders[histogramShader_].shaderHandle());

            descriptors.write(DescriptorSets::SetType::Static, descriptorSetIndex, 2, *keysIn)
                .write(DescriptorSets::SetType::Static, descriptorSetIndex, 3, scratch.histograms)
                .flushWrites()
                .bind(pass, descriptorSetIndex);
            struct {
                uint32_t numInstances;
                uint32_t bitOffset;
            } pc{instanceCount, bitOffset};
            pass.pushConstant(kPushRange, &pc);
            pass.dispatchCompute({numWorkgroups, 1, 1});
            pass.end();
        }
        barrier(scratch.histograms,
                Gpu::AccessFlagBit::ShaderStorageWriteBit,
                Gpu::AccessFlags(Gpu::AccessFlagBit::ShaderStorageReadBit |
                                 Gpu::AccessFlagBit::ShaderStorageWriteBit));

        // Scan
        {
            pass.bindShader(shaders[histogramShader_].shaderHandle());
            descriptors
                .write(DescriptorSets::SetType::Static, descriptorSetIndex, 2, scratch.histograms)
                .flushWrites()
                .bind(pass, descriptorSetIndex);
            struct {
                uint32_t numWorkgroups;
                uint32_t pad;
            } pc{numWorkgroups, 0u};
            pass.pushConstant(kPushRange, &pc);
            pass.dispatchCompute({1, 1, 1});
            pass.end();
        }
        barrier(scratch.histograms,
                Gpu::AccessFlagBit::ShaderStorageWriteBit,
                Gpu::AccessFlagBit::ShaderStorageReadBit);

        // Scatter
        {
            pass.bindShader(shaders[histogramShader_].shaderHandle());
            descriptors.write(DescriptorSets::SetType::Static, descriptorSetIndex, 2, *keysIn)
                .write(DescriptorSets::SetType::Static, descriptorSetIndex, 3, *indicesIn)
                .write(DescriptorSets::SetType::Static, descriptorSetIndex, 4, *keysOut)
                .write(DescriptorSets::SetType::Static, descriptorSetIndex, 5, *indicesOut)
                .write(DescriptorSets::SetType::Static, descriptorSetIndex, 6, scratch.histograms)
                .flushWrites()
                .bind(pass, descriptorSetIndex);
            struct {
                uint32_t numInstances;
                uint32_t bitOffset;
            } pc{instanceCount, bitOffset};
            pass.pushConstant(kPushRange, &pc);
            pass.dispatchCompute({numWorkgroups, 1, 1});
            pass.end();
        }
        barrier(*keysOut,
                Gpu::AccessFlagBit::ShaderStorageWriteBit,
                Gpu::AccessFlagBit::ShaderStorageReadBit);
        barrier(*indicesOut,
                Gpu::AccessFlagBit::ShaderStorageWriteBit,
                Gpu::AccessFlagBit::ShaderStorageReadBit);

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
} // namespace Cory
