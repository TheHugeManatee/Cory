#pragma once

#include <cstdint>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

namespace Cory {

struct graphics_context {
    vk::DispatchLoaderDynamic dl; // the vulkan dynamic dispatch loader
    vk::UniqueInstance instance{};
    vk::PhysicalDevice physicalDevice{};
    VmaAllocator allocator{};

    vk::UniqueDevice device{};
    vk::UniqueCommandPool transientCmdPool{};
    vk::UniqueCommandPool permanentCmdPool{};

    vk::Queue graphicsQueue{};
    vk::Queue presentQueue{};
};

} // namespace Cory