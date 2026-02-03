#include <Cory/Renderer/Gpu.hpp>

#include <fmt/format.h>
#include <stdexcept>

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_to_string.hpp>

namespace Cory {

Gpu::TextureAspectFlags flagsForFormat(TextureFormat format)
{
    using namespace KDGpu;
    switch (format) {
    // Color formats
    case Format::R8G8B8A8_UNORM:
    case Format::R8G8B8A8_SRGB:
    case Format::B8G8R8A8_UNORM:
    case Format::B8G8R8A8_SRGB:
    case Format::R8G8B8_UNORM:
    case Format::R8G8B8_SRGB:
    case Format::B8G8R8_UNORM:
    case Format::B8G8R8_SRGB:
    case Format::R32G32B32A32_SFLOAT:
    case Format::R16G16B16A16_SFLOAT:
        return TextureAspectFlagBits::ColorBit;
    // Depth formats
    case Format::D32_SFLOAT:
    case Format::D16_UNORM:
        return TextureAspectFlagBits::DepthBit;
    // Stencil format
    case Format::S8_UINT:
        return TextureAspectFlagBits::StencilBit;
    // Depth-stencil formats
    case Format::D24_UNORM_S8_UINT:
    case Format::D32_SFLOAT_S8_UINT:
    case Format::D16_UNORM_S8_UINT:
        return TextureAspectFlagBits::DepthBit | TextureAspectFlagBits::StencilBit;
    case Format::R32_SFLOAT:
    case Format::R16_SFLOAT:
        return TextureAspectFlagBits::ColorBit;
    default:
        throw std::runtime_error(fmt::format("flagsForFormat: Unsupported format {}",
                                             vk::to_string(static_cast<vk::Format>(format))));
    }
}
} // namespace Cory