#pragma once

#include <Cory/Framegraph/Common.hpp>

namespace Cory::Framegraph {

class Commands : NoCopy {
  public:
    Commands(Context &ctx, Magnum::Vk::CommandBuffer&cmdBuffer);

    Commands &bind(PipelineHandle pipeline);

    Commands &setupDynamicStates(const DynamicStates &dynamicStates);

    Commands &endPass();

    Magnum::Vk::CommandBuffer &handle() { return *cmdBuffer_; };

    Magnum::Vk::CommandBuffer *operator->() { return cmdBuffer_; };

  private:
    Context *ctx_;
    Magnum::Vk::CommandBuffer *cmdBuffer_;
};

} // namespace Cory::Framegraph
