#include "vk/command_pool.h"

#include "vk/graphics_context.h"

namespace cory {
namespace vk {

void command_pool::reset(VkCommandPoolResetFlags flags /*= {}*/)
{
    vkResetCommandPool(ctx_.device(), get(), flags);
}

cory::vk::command_pool command_pool_builder::create()
{
    VkCommandPool pool;
    VK_CHECKED_CALL(vkCreateCommandPool(ctx_.device(), &info_, nullptr, &pool),
                    "Pool could not create new VkCommandPool element!");

    return command_pool(ctx_, make_shared_resource(pool, [dev = ctx_.device()](VkCommandPool p) {
                            vkDestroyCommandPool(dev, p, nullptr);
                        }));

    //    return ctx_.pool<command_pool>().get(*this);
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

    command_pool pool = command_pool_builder(ctx).create();
    command_pool pool2 = pool;
    CO_CORE_WARN("Test with command pools");

    //ctx.pool<command_pool>().recycle(std::move(pool));
}

#endif