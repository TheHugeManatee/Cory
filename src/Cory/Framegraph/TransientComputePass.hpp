#pragma once

#include <Cory/Framegraph/Common.hpp>
#include <Cory/Renderer/Common.hpp>
#include <Cory/Renderer/Gpu.hpp>

#include <KDGpu/pipeline_layout_options.h>

namespace Cory {

struct ComputePassDeclaration {
    std::string name;
    ShaderHandle shader;
};

class TransientComputePass {
  public:
    explicit TransientComputePass(Context &ctx,
                                  FramegraphResourceManager &textures,
                                  ComputePassDeclaration pass);
    ~TransientComputePass();

    TransientComputePass(TransientComputePass &&) = default;
    TransientComputePass &operator=(TransientComputePass &&) = default;

    /// begin a render pass. automatically binds the shader binding context
    Gpu::ComputePassCommandRecorder begin(const RenderInput &renderApi);

    /**
     * Ends the render pass.
     * @param recorder must provide the recorder obtained from begin() via move semantics
     */
    void end(Gpu::RenderPassCommandRecorder &&recorder);

    /// Obtain the pipeline layout handle. Creates the layout if necessary.
    [[nodiscard]] Gpu::PipelineLayoutHandle pipelineLayoutHandle() noexcept;

  private:
    bool wasEnded_;
    Context *ctx_;
    FramegraphResourceManager *textures_;

    ComputePassDeclaration pass_;

    Gpu::ComputePipelineHandle pipeline_;
    Gpu::PipelineLayoutHandle pipelineLayout_;
    const RenderInput *currentRenderApi_;
};

} // namespace Cory
