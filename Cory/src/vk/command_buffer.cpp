#include "vk/command_buffer.h"

namespace cory::vk {

}

#ifndef DOCTEST_CONFIG_DISABLE

#include <doctest/doctest.h>

#include "cory/vk/graphics_context.h"
#include "cory/vk/test_utils.h"

TEST_CASE("basic command buffer usage")
{
    using namespace cory::vk;
    graphics_context &ctx = test_context();

    command_pool pool = command_pool_builder(ctx).create();

    VkCommandBuffer vk_cmd_buffer;
    VkCommandBufferAllocateInfo cmdAllocInfo = {};
    cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAllocInfo.pNext = nullptr;

    // commands will be made from our _commandPool
    cmdAllocInfo.commandPool = pool.get();
    // we will allocate 1 command buffer
    cmdAllocInfo.commandBufferCount = 1;
    // command level is Primary
    cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    CHECK_EQ(vkAllocateCommandBuffers(ctx.device(), &cmdAllocInfo, &vk_cmd_buffer), VK_SUCCESS);

    command_buffer cmd_buffer(pool);
    cmd_buffer.bind_pipeline({}, {});
    executable_command_buffer exec_cmd_buffer = cmd_buffer.end();
}

#endif