#include <Cory/Framegraph/CommandList.hpp>

#include <Cory/Base/Log.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/ResourceManager.hpp>

#include <Magnum/Vk/CommandBuffer.h>
#include <Magnum/Vk/Device.h>
#include <Magnum/Vk/Pipeline.h>

namespace Cory::Framegraph {

namespace /* detail */ {

VkCullModeFlags getVkCullMode(CullMode cullMode)
{
    switch (cullMode) {
    case CullMode::None:
        return VK_CULL_MODE_NONE;
    case CullMode::Front:
        return VK_CULL_MODE_FRONT_BIT;
    case CullMode::Back:
        return VK_CULL_MODE_BACK_BIT;
    case CullMode::FrontAndBack:
        return VK_CULL_MODE_FRONT_AND_BACK;
    }
    throw std::invalid_argument{"Unknown cull mode value!"};
}
VkCompareOp getVkCompareOp(DepthTest test)
{
    switch (test) {
    case DepthTest::Less:
        return VK_COMPARE_OP_LESS;
    case DepthTest::Greater:
        return VK_COMPARE_OP_GREATER;
    case DepthTest::LessOrEqual:
        return VK_COMPARE_OP_LESS_OR_EQUAL;
    case DepthTest::GreaterOrEqual:
        return VK_COMPARE_OP_GREATER_OR_EQUAL;
    case DepthTest::Always:
        return VK_COMPARE_OP_ALWAYS;
    case DepthTest::Never:
        return VK_COMPARE_OP_NEVER;
    case DepthTest::Disabled:
        return VK_COMPARE_OP_ALWAYS;
    }
    throw std::invalid_argument{"Invalid depth test value"};
}

} // namespace

CommandList::CommandList(Context &ctx, Magnum::Vk::CommandBuffer &cmdBuffer)
    : ctx_(&ctx)
    , cmdBuffer_{&cmdBuffer}
{
}

CommandList &CommandList::bind(PipelineHandle pipeline)
{
    cmdBuffer_->bindPipeline(ctx_->resources()[pipeline]);
    return *this;
}

CommandList &CommandList::setupDynamicStates(const DynamicStates &dynamicStates)
{
    CO_CORE_ASSERT(!(dynamicStates.renderArea.offset.x == 0 &&
                     dynamicStates.renderArea.offset.y == 0 &&
                     dynamicStates.renderArea.extent.width == 0 &&
                     dynamicStates.renderArea.extent.height == 0),
                   "renderArea cannot be zero!");
    { // set up viewport and scissor
        VkViewport viewport{
            .x = gsl::narrow_cast<float>(dynamicStates.renderArea.offset.x),
            .y = gsl::narrow_cast<float>(dynamicStates.renderArea.offset.y),
            .width = gsl::narrow_cast<float>(dynamicStates.renderArea.extent.width),
            .height = gsl::narrow_cast<float>(dynamicStates.renderArea.extent.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };
        ctx_->device()->CmdSetViewport(*cmdBuffer_, 0, 1, &viewport);
        ctx_->device()->CmdSetScissor(*cmdBuffer_, 0, 1, &dynamicStates.renderArea);
    }
    { // set up cull mode
        VkCullModeFlags cullMode = getVkCullMode(dynamicStates.cullMode);
        ctx_->device()->CmdSetCullMode(*cmdBuffer_, cullMode);
    }
    // set up depth test
    if (dynamicStates.depthTest != DepthTest::Disabled) {
        VkCompareOp depthCompare = getVkCompareOp(dynamicStates.depthTest);
        ctx_->device()->CmdSetDepthTestEnable(*cmdBuffer_, VK_TRUE);
        ctx_->device()->CmdSetDepthCompareOp(*cmdBuffer_, depthCompare);
    }
    else {
        ctx_->device()->CmdSetDepthTestEnable(*cmdBuffer_, VK_FALSE);
    }
    // set up depth write
    ctx_->device()->CmdSetDepthWriteEnable(
        *cmdBuffer_, dynamicStates.depthWrite == DepthWrite::Enabled ? VK_TRUE : VK_FALSE);
    return *this;
}

CommandList &CommandList::beginRenderPass(PipelineHandle pipelineHandle,
                                          const VkRenderingInfo *renderingInfo)
{
    cmdBuffer_->bindPipeline(ctx_->resources()[pipelineHandle]);

    ctx_->device()->CmdBeginRendering(*cmdBuffer_, renderingInfo);
    return *this;
}
CommandList &CommandList::endPass()
{
    // todo sanity checks?
    ctx_->device()->CmdEndRendering(*cmdBuffer_);
    return *this;
}

} // namespace Cory::Framegraph