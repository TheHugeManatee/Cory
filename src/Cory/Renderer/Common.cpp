#include "Cory/Renderer/Common.hpp"

#include <Magnum/Vk/Image.h>

namespace Vk = Magnum::Vk;

namespace Cory {

bool isColorFormat(PixelFormat format)
{
    return bool(imageAspectsFor(format) & Vk::ImageAspect::Color);
}

bool isDepthFormat(PixelFormat format)
{
    return bool(imageAspectsFor(format) & Vk::ImageAspect::Depth);
}

bool isStencilFormat(PixelFormat format)
{
    return bool(imageAspectsFor(format) & Vk::ImageAspect::Stencil);
}

}