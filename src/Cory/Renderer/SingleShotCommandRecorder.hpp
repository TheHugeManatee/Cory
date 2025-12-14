/*
 * Copyright 2022 OneProjects Design Innovation Limited
 * Company Number 606427, Ireland
 * All rights reserved
 */

#pragma once

#include <Cory/Renderer/Common.hpp>
#include <Cory/Renderer/VulkanUtils.hpp>

#include <KDGpu/command_recorder.h>

namespace Cory {

/**
 * A command buffer that is immediately submitted to the graphics queue on destruction.
 * This will wait (stall the CPU) until the command buffer has finished executing so it is not
 * intended to perform per-frame operations but rather to perform operations like resource
 * creation/initialization etc. in the app initialization phase.
 */
class SingleShotCommandRecorder : NoCopy {
  public:
    SingleShotCommandRecorder(Context &ctx);
    ~SingleShotCommandRecorder();

    // movable
    SingleShotCommandRecorder(SingleShotCommandRecorder &&) = default;
    SingleShotCommandRecorder &operator=(SingleShotCommandRecorder &&) = default;

    operator CommandRecorder &() { return buffer(); }

    CommandRecorder &buffer() { return commandRecorder_; }

    CommandRecorder *operator->() { return &commandRecorder_; };

  private:
    Context *ctx_;
    CommandRecorder commandRecorder_;
};
static_assert(std::movable<SingleShotCommandRecorder>);

} // namespace Cory
