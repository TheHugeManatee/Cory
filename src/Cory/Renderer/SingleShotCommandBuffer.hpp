/*
 * Copyright 2022 OneProjects Design Innovation Limited
 * Company Number 606427, Ireland
 * All rights reserved
 */

#pragma once

#include <Cory/Renderer/Common.hpp>
#include <Cory/Renderer/VulkanUtils.hpp>

#include <Magnum/Vk/CommandBuffer.h>

#include <memory>

namespace Cory {

/**
 * A command buffer that is immediately submitted to the graphics queue on destruction.
 * This will wait (stall the CPU) until the command buffer has finished executing so it is not
 * intended to perform per-frame operations but rather to perform operations like resource
 * creation/initialization etc. in the app initialization phase.
 */
class SingleShotCommandBuffer : NoCopy {
  public:
    SingleShotCommandBuffer(Context &ctx);
    ~SingleShotCommandBuffer();

    // movable
    SingleShotCommandBuffer(SingleShotCommandBuffer&&) = default;
    SingleShotCommandBuffer& operator=(SingleShotCommandBuffer&&) = default;

    operator VkCommandBuffer() { return buffer(); }

    Magnum::Vk::CommandBuffer &buffer() { return commandBuffer_; }

    Magnum::Vk::CommandBuffer *operator->() { return &commandBuffer_; };

  private:
    Context *ctx_;
    Magnum::Vk::CommandBuffer commandBuffer_;
};
static_assert(std::movable<SingleShotCommandBuffer>);

} // namespace Cory
