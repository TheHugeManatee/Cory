#pragma once


#include <Magnum/Vk/PixelFormat.h>
#include <MagnumExternal/Vulkan/flextVk.h>

namespace Cory {


VkFormat fromMagnum(Magnum::Vk::PixelFormat format) {
    return static_cast<VkFormat>(format);
}

Magnum::Vk::PixelFormat fromVk(VkFormat format) {
    return static_cast<Magnum::Vk::PixelFormat>(format);
}

}