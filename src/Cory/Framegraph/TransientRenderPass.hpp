#pragma once

#include <Cory/Framegraph/Common.hpp>
#include <Cory/Renderer/Gpu.hpp>

// Todo can live with forward decl only?
#include <KDGpu/pipeline_layout_options.h>

#include <KDGpu/pipeline_layout.h>

#include <string_view>
#include <vector>

namespace Cory {

enum class MeshInput { Enabled, Disabled };

struct TransientRenderPassInfo {
    int32_t sampleCount;
    std::vector<Gpu::TextureHandle> colorAttachments;
    Gpu::TextureHandle depthAttachment;
    Gpu::TextureHandle stencilAttachment;
};

struct ColorAttachment {
    TransientTextureHandle target;
    Gpu::AttachmentLoadOperation load;
    Gpu::AttachmentStoreOperation store;
    Gpu::ColorClearValue clearColor;
};
struct DepthStencilAttachment {
    TransientTextureHandle target;
    Gpu::AttachmentLoadOperation load;
    Gpu::AttachmentStoreOperation store;
    Gpu::DepthStencilClearValue clearDepthStencil;
};
struct RenderPassDeclaration {
    std::string name;
    std::vector<ShaderHandle> shaders;
    std::vector<ColorAttachment> attachments;
    std::optional<DepthStencilAttachment> depthAttachment;
    std::optional<DepthStencilAttachment> stencilAttachment;
    std::vector<Gpu::PushConstantRange> pushConstantRanges;

    DynamicStates dynamicStates;
    MeshInput meshInput{MeshInput::Enabled};
};

/// Transient render stores the information to set up and execute a render pass
class TransientRenderPass : NoCopy {
  public:
    explicit TransientRenderPass(Context &ctx,
                                 TextureManager &textures,
                                 RenderPassDeclaration pass);
    ~TransientRenderPass();

    TransientRenderPass(TransientRenderPass &&) = default;
    TransientRenderPass &operator=(TransientRenderPass &&) = default;

    /**
     * starts the rendering and sets up the render pass according to
     * the information described in the builder.
     *
     *  1. Binds a pipeline with the required layout -
     *  2. Calls begin() on the render pass with the attachments
     *  3. Set up the dynamic state (Depth test, cull mode, ...) as set up in the builder TODO
     */
    Gpu::RenderPassCommandRecorder begin(CommandRecorder &cmd);

    /// Obtain the pipeline layout handle. Creates the layout if necessary.
    [[nodiscard]] Gpu::PipelineLayoutHandle pipelineLayoutHandle() noexcept;

    /// Obtain the pipeline handle for the graphics pipeline associated with this pass. Creates the
    /// pipeline if necessary.
    [[nodiscard]] Gpu::GraphicsPipelineHandle pipelineHandle() noexcept;

  private:
    Gpu::SampleCountFlagBits determineSampleCount() const;
    Gpu::Rect2D determineRenderArea() const;

    Context *ctx_;
    TextureManager *textures_;

    RenderPassDeclaration pass_;

    DynamicStates dynamicStates_;

    Gpu::GraphicsPipelineHandle pipeline_;
    Gpu::PipelineLayoutHandle pipelineLayout_;
};

} // namespace Cory
