#pragma once

#include <Cory/Framegraph/Common.hpp>

namespace Cory {

/**
 * Effectively a wrapper over a Command Buffer, but it understands operations on more high-level objects such as PipelineHandles, DescriptorSetManagers etc
 */
class CommandList : NoCopy {
  public:
    CommandList(Context &ctx, Magnum::Vk::CommandBuffer &cmdBuffer);

    CommandList &bind(PipelineHandle pipeline);

    CommandList &setupDynamicStates(const DynamicStates &dynamicStates);

    Magnum::Vk::CommandBuffer &handle() { return *cmdBuffer_; };

    Magnum::Vk::CommandBuffer *operator->() { return cmdBuffer_; };

    CommandList &beginRenderPass(PipelineHandle pipelineHandle, const VkRenderingInfo *renderingInfo);
    CommandList &endPass();

  private:
    Context *ctx_;
    Magnum::Vk::CommandBuffer *cmdBuffer_;
};

} // namespace Cory
