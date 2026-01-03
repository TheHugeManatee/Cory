#pragma once

#include <Cory/Framegraph/TransientComputePass.hpp>
#include <Cory/Renderer/Common.hpp>
#include <Cory/Renderer/Gpu.hpp>
#include <Cory/Renderer/ShaderManager.hpp>

#include <KDGpu/bind_group.h>
#include <KDGpu/bind_group_pool.h>
#include <KDGpu/buffer.h>

#include <vector>

namespace Cory {
class FramegraphResourceManager;
class RenderTaskBuilder;

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
        Gpu::BindGroupPool bindGroupPool;
        std::vector<Gpu::BindGroup> bindGroups;
        Gpu::DeviceSize capacity{0};
        uint32_t workgroups{0};
    };

    struct Passes {
        TransientComputePass histogram;
        TransientComputePass scan;
        TransientComputePass scatter;
    };

    Passes declarePasses(RenderTaskBuilder &builder);
    Passes declarePasses(FramegraphResourceManager &resources);

    ScratchBuffers &scratchForFrame(uint32_t frameIndex, uint32_t instanceCount);

    /// Dispatches the histogram pass for the given keys buffer.
    void dispatchHistogram(Gpu::CommandRecorder &cmd,
                           ScratchBuffers &scratch,
                           TransientComputePass &pass,
                           const Gpu::Buffer &keys,
                           uint32_t instanceCount,
                           uint32_t bitOffset,
                           uint32_t frameInFlightIndex);

    /// Dispatches the scan pass over the histogram buffer.
    void dispatchScan(Gpu::CommandRecorder &cmd,
                      ScratchBuffers &scratch,
                      TransientComputePass &pass,
                      uint32_t frameInFlightIndex);

    /// Dispatches the scatter pass into the provided output buffers.
    void dispatchScatter(Gpu::CommandRecorder &cmd,
                         ScratchBuffers &scratch,
                         TransientComputePass &pass,
                         const Gpu::Buffer &keysIn,
                         const Gpu::Buffer &indicesIn,
                         Gpu::Buffer &keysOut,
                         Gpu::Buffer &indicesOut,
                         uint32_t instanceCount,
                         uint32_t bitOffset,
                         uint32_t frameInFlightIndex);

    /// Runs the GPU radix sort over the predicate values (uint keys). Returns the sorted indices.
    Gpu::Buffer &sort(Gpu::CommandRecorder &cmd,
                      ScratchBuffers &scratch,
                      Passes &passes,
                      const Gpu::Buffer &predicateBuffer,
                      uint32_t instanceCount,
                      Gpu::Buffer &outputIndices,
                      uint32_t frameInFlightIndex);

  private:
    void ensureScratch(ScratchBuffers &scratch, uint32_t instanceCount, uint32_t scratchIndex);

    Context *ctx_{nullptr};

    ShaderHandle histogramShader_;
    ShaderHandle scanShader_;
    ShaderHandle scatterShader_;

    std::vector<ScratchBuffers> perFrameScratch_;
};
} // namespace Cory
