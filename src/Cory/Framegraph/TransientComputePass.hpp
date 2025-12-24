#pragma once

#include <Cory/Framegraph/Common.hpp>
#include <Cory/Renderer/Common.hpp>
#include <Cory/Renderer/Gpu.hpp>

#include <KDGpu/pipeline_layout_options.h>

namespace Cory {

struct ComputePassDeclaration {
    std::string name;
    ShaderHandle shader;
    std::vector<Gpu::PushConstantRange> pushConstantRanges;
};

class TransientComputePass {
  public:
    explicit TransientComputePass(Context &ctx,
                                  TextureManager &textures,
                                  ComputePassDeclaration pass);
    ~TransientComputePass();

    TransientComputePass(TransientComputePass &&) = default;
    TransientComputePass &operator=(TransientComputePass &&) = default;

    ///
    Gpu::ComputePassCommandRecorder begin(CommandRecorder &cmd);

    /// Obtain the pipeline layout handle. Creates the layout if necessary.
    [[nodiscard]] Gpu::PipelineLayoutHandle pipelineLayoutHandle() noexcept;

    /// Obtain the pipeline handle for the graphics pipeline associated with this pass. Creates the
    /// pipeline if necessary.
    [[nodiscard]] Gpu::ComputePipelineHandle pipelineHandle() noexcept;

  private:
    Context *ctx_;
    TextureManager *textures_;

    ComputePassDeclaration pass_;

    Gpu::ComputePipelineHandle pipeline_;
    Gpu::PipelineLayoutHandle pipelineLayout_;
};

} // namespace Cory
