#pragma once

#include <Cory/Framegraph/Common.hpp>
#include <Cory/Renderer/Gpu.hpp>

#include <string_view>
#include <vector>

namespace Cory {

struct TransientRenderPassInfo {
    int32_t sampleCount;
    std::vector<Gpu::TextureHandle> colorAttachments;
    Gpu::TextureHandle depthAttachment;
    Gpu::TextureHandle stencilAttachment;
};

struct AttachmentKind {
    VkAttachmentLoadOp loadOp;
    VkAttachmentStoreOp storeOp;
    VkClearValue clearValue;
};

class TransientRenderPass : NoCopy {
  public:
    ~TransientRenderPass();

    TransientRenderPass(TransientRenderPass &&) = default;
    TransientRenderPass &operator=(TransientRenderPass &&) = default;

    /**
     * starts the rendering and sets up the render pass according to
     * the information described in the builder.
     *
     *  1. Binds a pipeline with the required layout -
     *  2. Calls CmdBeginRendering with the attachments
     *  3. Set up the dynamic state (Depth test, cull mode, ...) as set up in the builder
     */
    void begin(CommandRecorder &cmd);

    void end(CommandRecorder &cmd);

  private:
    friend class TransientRenderPassBuilder;
    TransientRenderPass(Context &ctx, std::string_view name, TextureManager &textures);

    int32_t determineSampleCount() const;
    VkRenderingAttachmentInfo makeAttachmentInfo(Gpu::TextureHandle handle,
                                                 AttachmentKind attachmentKind);

    Context *ctx_;
    std::string_view name_;
    TextureManager *textures_;

    std::vector<ShaderHandle> shaders_;
    std::vector<std::pair<Gpu::TextureHandle, AttachmentKind>> colorAttachments_;
    std::optional<std::pair<Gpu::TextureHandle, AttachmentKind>> depthAttachment_;
    std::optional<std::pair<Gpu::TextureHandle, AttachmentKind>> stencilAttachment_;

    DynamicStates dynamicStates_;
    bool hasMeshInput_{true}; // by default, uses the default mesh layout

    Gpu::GraphicsPipelineHandle handle_;
    bool hasBegun_{false}; ///< only needed for diagnostics
    VkRect2D determineRenderArea();
};

class TransientRenderPassBuilder : NoCopy, NoMove {
  public:
    TransientRenderPassBuilder(Context &ctx, std::string_view name, TextureManager &textures);

    ~TransientRenderPassBuilder();

    TransientRenderPassBuilder &shaders(std::vector<ShaderHandle> shaders);

    TransientRenderPassBuilder &attach(TransientTextureHandle handle,
                                       VkAttachmentLoadOp loadOp,
                                       VkAttachmentStoreOp storeOp,
                                       VkClearColorValue clearValue);
    TransientRenderPassBuilder &attachDepth(TransientTextureHandle handle,
                                            VkAttachmentLoadOp loadOp,
                                            VkAttachmentStoreOp storeOp,
                                            float clearValue);
    TransientRenderPassBuilder &attachStencil(TransientTextureHandle handle,
                                              VkAttachmentLoadOp loadOp,
                                              VkAttachmentStoreOp storeOp,
                                              uint32_t clearValue);

    /// create a render pass that does not expect any mesh to be attached
    TransientRenderPassBuilder &disableMeshInput();

    TransientRenderPass finish();

  private:
    TransientRenderPass renderPass_;
};

} // namespace Cory
