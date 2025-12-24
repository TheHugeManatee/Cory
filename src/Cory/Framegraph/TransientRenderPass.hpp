#pragma once

#include <Cory/Base/BitField.hpp>
#include <Cory/Framegraph/Common.hpp>
#include <Cory/Renderer/Gpu.hpp>

#include <KDGpu/graphics_pipeline_options.h>
#include <KDGpu/pipeline_layout_options.h>

#include <optional>
#include <vector>

namespace Cory {

// Options general options for the render pass
enum class PassOptionFlagBits {
    None = 0,
    /// Skip creating/binding the pipeline on TransientRenderPass::begin(), in order to allow
    /// custom pipeline binding/switching (mostly interop with other libraries, e.g. imgui)
    SkipPipelineBind = 1 << 0,
    // Disable binding of mesh input (vertex/index buffers) when beginning the render pass
    DisableMeshInput = 2 << 1,
};
using PassOptionFlags = BitField<PassOptionFlagBits>;

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
    std::optional<Gpu::BlendOptions> blend;
};
struct DepthStencilAttachment {
    TransientTextureHandle target;
    Gpu::AttachmentLoadOperation load;
    Gpu::AttachmentStoreOperation store;
    Gpu::DepthStencilClearValue clearDepthStencil;
};
struct RenderPassDeclaration {
    std::string name;
    PassOptionFlags options{PassOptionFlagBits::None};

    std::vector<ShaderHandle> shaders; // Unused if SkipPipelineBind is used
    std::vector<ColorAttachment> attachments;
    std::optional<DepthStencilAttachment> depthAttachment;
    std::optional<DepthStencilAttachment> stencilAttachment;
    std::vector<Gpu::PushConstantRange> pushConstantRanges; // Unused if SkipPipelineBind is used
    std::optional<Gpu::VertexOptions> vertexOptions;

    DynamicStates dynamicStates;
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
