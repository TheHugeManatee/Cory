/*
 * Copyright 2022 OneProjects Design Innovation Limited
 * Company Number 606427, Ireland
 * All rights reserved
 */

#include <Cory/Renderer/SingleShotCommandRecorder.hpp>

#include <Cory/Renderer/Context.hpp>
#include <KDGpu/vulkan/vulkan_graphics_api.h>

namespace Cory {

SingleShotCommandRecorder::SingleShotCommandRecorder(Context &ctx)
    : ctx_{&ctx}
    , commandRecorder_{ctx_->device().createCommandRecorder()}
{
}

SingleShotCommandRecorder::~SingleShotCommandRecorder()
{
    auto command_buffer = commandRecorder_.finish();

    auto fence = ctx_->createFence("SingleShotCommandRecorder", FenceCreateMode::Unsignaled);

    ctx_->graphicsQueue().submit(Gpu::SubmitOptions{
        .commandBuffers = {command_buffer.handle()},
        .waitSemaphores = {},
        .signalSemaphores = {},
        .signalFence = {fence.handle()},
    });

    fence.wait();
}

} // namespace Cory
