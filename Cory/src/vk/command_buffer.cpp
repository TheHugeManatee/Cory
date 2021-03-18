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

    command_buffer cmd_buffer = pool.allocate_buffer();

    //cmd_buffer.bind_pipeline({}, {});
    //executable_command_buffer exec_cmd_buffer = cmd_buffer.end();
}

#endif