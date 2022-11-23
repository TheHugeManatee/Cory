#pragma once

#include <Cory/Framegraph/Common.hpp>

namespace Cory::Framegraph {

class BeginRenderingBuilder : NoCopy, NoMove {
  public:
    BeginRenderingBuilder(Context &ctx, Magnum::Vk::CommandBuffer &cmdBuffer);

    ~BeginRenderingBuilder();

    BeginRenderingBuilder &attach(TextureHandle &handle,
                                  VkAttachmentLoadOp loadOp,
                                  VkAttachmentStoreOp storeOp,
                                  VkClearColorValue clearValue);
    BeginRenderingBuilder &attachDepth(TextureHandle &handle,
                                       VkAttachmentLoadOp loadOp,
                                       VkAttachmentStoreOp storeOp,
                                       VkClearDepthStencilValue clearValue);
    BeginRenderingBuilder &attachStencil(TextureHandle &handle,
                                         VkAttachmentLoadOp loadOp,
                                         VkAttachmentStoreOp storeOp,
                                         VkClearDepthStencilValue clearValue);

    void begin();

  private:
    VkRenderingAttachmentInfo makeAttachmentInfo(TextureHandle &handle,
                                                 VkAttachmentLoadOp loadOp,
                                                 VkAttachmentStoreOp storeOp,
                                                 VkClearValue clearValue);
    Context &ctx_;
    Magnum::Vk::CommandBuffer &cmdBuffer_;
    VkRect2D renderArea_;
    std::vector<VkRenderingAttachmentInfo> colorAttachments_;
    std::optional<VkRenderingAttachmentInfo> depthAttachment_;
    std::optional<VkRenderingAttachmentInfo> stencilAttachment_;
    bool hasBegun_{false}; ///< only needed for diagnostics
};

class CommandList : NoCopy {
  public:
    CommandList(Context &ctx, Magnum::Vk::CommandBuffer &cmdBuffer);

    CommandList &bind(PipelineHandle pipeline);

    CommandList &setupDynamicStates(const DynamicStates &dynamicStates);


    Magnum::Vk::CommandBuffer &handle() { return *cmdBuffer_; };

    Magnum::Vk::CommandBuffer *operator->() { return cmdBuffer_; };

    BeginRenderingBuilder setupRenderPass() { return BeginRenderingBuilder{*ctx_, *cmdBuffer_}; }
    CommandList &endPass();

  private:
    Context *ctx_;
    Magnum::Vk::CommandBuffer *cmdBuffer_;
};

} // namespace Cory::Framegraph
