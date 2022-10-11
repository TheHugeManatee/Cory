/*
 * Copyright 2022 OneProjects Design Innovation Limited
 * Company Number 606427, Ireland
 * All rights reserved
 */

#include <Cory/RenderCore/SingleShotCommandBuffer.hpp>

#include <Cory/RenderCore/Context.hpp>

#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Containers/Reference.h>
#include <Magnum/Vk/CommandPool.h>
#include <Magnum/Vk/Queue.h>

namespace Cory {

SingleShotCommandBuffer::SingleShotCommandBuffer(Context &ctx)
    : ctx_{ctx}
    , commandBuffer_{ctx_.commandPool().allocate()}
{
    commandBuffer_.begin();
}

SingleShotCommandBuffer::~SingleShotCommandBuffer()
{
    commandBuffer_.end();

    auto fence =
        ctx_.graphicsQueue().submit({Magnum::Vk::SubmitInfo{}.setCommandBuffers({commandBuffer_})});
    fence.wait();
}

} // namespace Cory