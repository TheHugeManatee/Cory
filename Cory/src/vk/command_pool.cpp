#include "vk/command_pool.h"

#include "vk/command_buffer.h"
#include "vk/graphics_context.h"

namespace cory {
namespace vk {

void command_pool::reset(VkCommandPoolResetFlags flags /*= {}*/)
{
    vkResetCommandPool(ctx_.device(), get(), flags);
}

cory::vk::command_buffer
command_pool::allocate_buffer(VkCommandBufferUsageFlags usageFlags /*= {}*/)
{
    VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    VkCommandBufferAllocateInfo cmdAllocInfo = {};
    cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAllocInfo.pNext = nullptr;

    cmdAllocInfo.commandPool = command_pool_ptr_.get();
    cmdAllocInfo.commandBufferCount = 1;
    cmdAllocInfo.level = level;

    VkCommandBuffer vk_cmd_buffer;
    VK_CHECKED_CALL(vkAllocateCommandBuffers(ctx_.device(), &cmdAllocInfo, &vk_cmd_buffer),
                    "Could not allocated command buffer");

    auto cmd_buf_ptr = make_shared_resource<VkCommandBuffer>(
        vk_cmd_buffer,
        [dev = ctx_.device(), pool = command_pool_ptr_.get()](VkCommandBuffer cmd_buffer) {
            vkFreeCommandBuffers(dev, pool, 1, &cmd_buffer);
        });

    // set the command buffer into begin state
    VkCommandBufferBeginInfo begin_info{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                        .flags = usageFlags};
    vkBeginCommandBuffer(cmd_buf_ptr.get(), &begin_info);

    return cory::vk::command_buffer{command_pool_ptr_, std::move(cmd_buf_ptr), target_queue_};
}

cory::vk::command_pool command_pool_builder::create()
{
    info_.queueFamilyIndex = target_queue_->family();

    VkCommandPool pool;
    VK_CHECKED_CALL(vkCreateCommandPool(ctx_.device(), &info_, nullptr, &pool),
                    "Pool could not create new VkCommandPool element!");

    auto vk_command_pool_ptr = make_shared_resource(
        pool, [dev = ctx_.device()](VkCommandPool p) { vkDestroyCommandPool(dev, p, nullptr); });

    return command_pool(ctx_, vk_command_pool_ptr, target_queue_);
}

// cory::vk::command_pool command_pool_pool::get(const command_pool_builder &builder)
//{
//    auto h = hash(builder);
//    if (available_elements_.contains(h)) {
//        auto handle = available_elements_.extract(h);
//        return handle.mapped();
//    }
//
//    VkCommandPool pool;
//    VK_CHECKED_CALL(vkCreateCommandPool(builder.ctx_.device(), &builder.info_, nullptr, &pool),
//                    "Pool could not create new VkCommandPool element!");
//
//    return command_pool(
//        builder.ctx_, h, make_shared_resource(pool, [dev = builder.ctx_.device()](VkCommandPool p)
//        {
//            vkDestroyCommandPool(dev, p, nullptr);
//        }));
//}

} // namespace vk
} // namespace cory

#ifndef DOCTEST_CONFIG_DISABLE

#include "Log.h"
#include "vk/test_utils.h"

#include <doctest/doctest.h>

TEST_CASE("Creating and releasing a command pool")
{
    using namespace cory::vk;
    graphics_context &ctx = test_context();

    command_pool pool = command_pool_builder(ctx).queue(ctx.graphics_queue()).create();
    command_pool pool2 = pool;
    CO_CORE_WARN("Test with command pools");

    // ctx.pool<command_pool>().recycle(std::move(pool));
}

#endif