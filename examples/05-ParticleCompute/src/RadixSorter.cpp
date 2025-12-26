#include "RadixSorter.hpp"

#include <Cory/Base/ResourceLocator.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/DescriptorSets.hpp>
#include <Cory/Renderer/PipelineCache.hpp>
#include <Cory/Renderer/ShaderManager.hpp>

#include <KDGpu/buffer_options.h>

#include <array>
#include <numeric>

namespace Cory {
namespace {
constexpr uint32_t kWorkgroupSize = 256u;
} // namespace

RadixSorter::RadixSorter(Context &ctx)
    : ctx_{&ctx}
{
    preprocessShader_ =
        ctx.shaders().createShader(ResourceLocator::Locate("sort_preprocess.comp.slang"));
    histogramShader_ =
        ctx.shaders().createShader(ResourceLocator::Locate("radix_sort_histogram.comp.slang"));
    scanShader_ = ctx.shaders().createShader(ResourceLocator::Locate("radix_sort_scan.comp.slang"));
    scatterShader_ =
        ctx.shaders().createShader(ResourceLocator::Locate("radix_sort_scatter.comp.slang"));
}

RadixSorter::~RadixSorter()
{
    if (!ctx_) return;
    auto &shaders = ctx_->shaders();
    shaders.release(preprocessShader_);
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

void RadixSorter::ensurePipelines()
{
    if (preprocessPipeline_.isValid()) return;

    auto &cache = ctx_->pipelineCache();
    auto layouts = ctx_->descriptors().layouts();

    auto makeLayout = [&](std::string_view label,
                          std::initializer_list<Gpu::PushConstantRange> ranges) {
        return cache.queryLayout(Gpu::PipelineLayoutOptions{
            .label = std::string{label},
            .bindGroupLayouts = layouts,
            .pushConstantRanges = std::vector<Gpu::PushConstantRange>{ranges},
        });
    };

    preprocessLayout_ =
        makeLayout("RadixPreprocessLayout",
                   {Gpu::PushConstantRange{.offset = 0,
                                           .size = sizeof(uint32_t),
                                           .shaderStages = Gpu::ShaderStageFlagBits::ComputeBit}});
    histogramLayout_ = makeLayout(
        "RadixHistogramLayout",
        {Gpu::PushConstantRange{
            .offset = 0, .size = 8, .shaderStages = Gpu::ShaderStageFlagBits::ComputeBit}});
    scanLayout_ =
        makeLayout("RadixScanLayout",
                   {Gpu::PushConstantRange{.offset = 0,
                                           .size = sizeof(uint32_t),
                                           .shaderStages = Gpu::ShaderStageFlagBits::ComputeBit}});
    scatterLayout_ = makeLayout(
        "RadixScatterLayout",
        {Gpu::PushConstantRange{
            .offset = 0, .size = 8, .shaderStages = Gpu::ShaderStageFlagBits::ComputeBit}});

    preprocessPipeline_ =
        cache.queryComputePipeline("RadixPreprocess",
                                   ComputePipelineDescriptor{.shader = preprocessShader_,
                                                             .pipelineLayout = preprocessLayout_});
    histogramPipeline_ = cache.queryComputePipeline(
        "RadixHistogram",
        ComputePipelineDescriptor{.shader = histogramShader_, .pipelineLayout = histogramLayout_});
    scanPipeline_ = cache.queryComputePipeline(
        "RadixScan",
        ComputePipelineDescriptor{.shader = scanShader_, .pipelineLayout = scanLayout_});
    scatterPipeline_ = cache.queryComputePipeline(
        "RadixScatter",
        ComputePipelineDescriptor{.shader = scatterShader_, .pipelineLayout = scatterLayout_});
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
        .usage = Gpu::BufferUsageFlagBits::StorageBufferBit,
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
                               const Gpu::Buffer &instanceBuffer,
                               const Gpu::Buffer &uboBuffer,
                               uint32_t instanceCount,
                               uint32_t descriptorSetIndex)
{
    ensurePipelines();
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

    // Initialize indicesA
    {
        auto *mapped = static_cast<uint32_t *>(scratch.indicesA.map());
        std::iota(mapped, mapped + instanceCount, 0u);
        scratch.indicesA.unmap();
    }

    // Preprocess
    {
        auto pass = cmd.beginComputePass({});
        pass.setPipeline(preprocessPipeline_);
        descriptors.write(DescriptorSets::SetType::Static, descriptorSetIndex, uboBuffer)
            .write(DescriptorSets::SetType::Static, descriptorSetIndex, 2, instanceBuffer)
            .write(DescriptorSets::SetType::Static, descriptorSetIndex, 3, scratch.keysA)
            .write(DescriptorSets::SetType::Static, descriptorSetIndex, 4, scratch.indicesA)
            .flushWrites()
            .bind(pass, descriptorSetIndex);
        pass.pushConstant({.offset = 0,
                           .size = sizeof(uint32_t),
                           .shaderStages = Gpu::ShaderStageFlagBits::ComputeBit},
                          &instanceCount,
                          preprocessLayout_);
        pass.dispatchCompute({numWorkgroups, 1, 1});
        pass.end();
    }
    barrier(scratch.keysA,
            Gpu::AccessFlagBit::ShaderStorageWriteBit,
            Gpu::AccessFlagBit::ShaderStorageReadBit);
    barrier(scratch.indicesA,
            Gpu::AccessFlagBit::ShaderStorageWriteBit,
            Gpu::AccessFlagBit::ShaderStorageReadBit);

    Gpu::Buffer *keysIn = &scratch.keysA;
    Gpu::Buffer *keysOut = &scratch.keysB;
    Gpu::Buffer *indicesIn = &scratch.indicesA;
    Gpu::Buffer *indicesOut = &scratch.indicesB;

    for (uint32_t bitOffset = 0; bitOffset < 32; bitOffset += 4) {
        // Histogram
        {
            auto pass = cmd.beginComputePass({});
            pass.setPipeline(histogramPipeline_);
            descriptors.write(DescriptorSets::SetType::Static, descriptorSetIndex, 2, *keysIn)
                .write(DescriptorSets::SetType::Static, descriptorSetIndex, 3, scratch.histograms)
                .flushWrites()
                .bind(pass, descriptorSetIndex);
            struct {
                uint32_t numInstances;
                uint32_t bitOffset;
            } pc{instanceCount, bitOffset};
            pass.pushConstant({.offset = 0,
                               .size = sizeof(pc),
                               .shaderStages = Gpu::ShaderStageFlagBits::ComputeBit},
                              &pc,
                              histogramLayout_);
            pass.dispatchCompute({numWorkgroups, 1, 1});
            pass.end();
        }
        barrier(scratch.histograms,
                Gpu::AccessFlagBit::ShaderStorageWriteBit,
                Gpu::AccessFlags(Gpu::AccessFlagBit::ShaderStorageReadBit |
                                 Gpu::AccessFlagBit::ShaderStorageWriteBit));

        // Scan
        {
            auto pass = cmd.beginComputePass({});
            pass.setPipeline(scanPipeline_);
            descriptors
                .write(DescriptorSets::SetType::Static, descriptorSetIndex, 2, scratch.histograms)
                .flushWrites()
                .bind(pass, descriptorSetIndex);
            pass.pushConstant({.offset = 0,
                               .size = sizeof(uint32_t),
                               .shaderStages = Gpu::ShaderStageFlagBits::ComputeBit},
                              &numWorkgroups,
                              scanLayout_);
            pass.dispatchCompute({1, 1, 1});
            pass.end();
        }
        barrier(scratch.histograms,
                Gpu::AccessFlagBit::ShaderStorageWriteBit,
                Gpu::AccessFlagBit::ShaderStorageReadBit);

        // Scatter
        {
            auto pass = cmd.beginComputePass({});
            pass.setPipeline(scatterPipeline_);
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
            pass.pushConstant({.offset = 0,
                               .size = sizeof(pc),
                               .shaderStages = Gpu::ShaderStageFlagBits::ComputeBit},
                              &pc,
                              scatterLayout_);
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

    return *indicesIn;
}
} // namespace Cory