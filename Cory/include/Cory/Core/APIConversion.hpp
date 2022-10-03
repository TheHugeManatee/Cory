#pragma once

#include <Magnum/Vk/PixelFormat.h>
#include <MagnumExternal/Vulkan/flextVk.h>
#include <glm/vec2.hpp>

namespace Cory {

inline VkFormat toVk(Magnum::Vk::PixelFormat format) { return static_cast<VkFormat>(format); }

inline Magnum::Vk::PixelFormat toMagnum(VkFormat format)
{
    return static_cast<Magnum::Vk::PixelFormat>(format);
}

inline VkExtent2D toVkExtent2D(glm::u32vec2 extent) { return {extent.x, extent.y}; }

} // namespace Cory