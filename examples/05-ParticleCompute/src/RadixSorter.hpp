#pragma once

#include <Cory/Renderer/Common.hpp>
#include <Cory/Renderer/Gpu.hpp>
#include <Cory/Renderer/ShaderManager.hpp>

#include <KDGpu/buffer.h>

#include <vector>

namespace Cory {
class RadixSorter {
  public:
    explicit RadixSorter(Context &ctx);
    ~RadixSorter();

    struct ScratchBuffers {
        Gpu::Buffer keysA;
        Gpu::Buffer keysB;
        Gpu::Buffer indicesA;
        Gpu::Buffer indicesB;
        Gpu::Buffer histograms;
        Gpu::DeviceSize capacity{0};
        uint32_t workgroups{0};
    };

    ScratchBuffers &scratchForFrame(uint32_t frameIndex, uint32_t instanceCount);

    /// Runs the GPU radix sort. Returns the buffer containing the sorted indices.
    Gpu::Buffer &sort(Gpu::CommandRecorder &cmd,
                      DescriptorSets &descriptors,
                      ScratchBuffers &scratch,
                      const Gpu::Buffer &instanceBuffer,
                      const Gpu::Buffer &uboBuffer,
                      uint32_t instanceCount,
                      uint32_t descriptorSetIndex);

  private:
    void ensurePipelines();
    void ensureScratch(ScratchBuffers &scratch, uint32_t instanceCount);

    Context *ctx_{nullptr};

    ShaderHandle preprocessShader_;
    ShaderHandle histogramShader_;
    ShaderHandle scanShader_;
    ShaderHandle scatterShader_;

    Gpu::PipelineLayoutHandle preprocessLayout_;
    Gpu::PipelineLayoutHandle histogramLayout_;
    Gpu::PipelineLayoutHandle scanLayout_;
    Gpu::PipelineLayoutHandle scatterLayout_;

    Gpu::ComputePipelineHandle preprocessPipeline_;
    Gpu::ComputePipelineHandle histogramPipeline_;
    Gpu::ComputePipelineHandle scanPipeline_;
    Gpu::ComputePipelineHandle scatterPipeline_;

    std::vector<ScratchBuffers> perFrameScratch_;
};
} // namespace Cory