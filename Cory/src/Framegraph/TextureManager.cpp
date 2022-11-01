#include <Cory/Framegraph/TextureManager.hpp>

#include <Cory/Base/Log.hpp>
#include <Cory/Base/FmtUtils.hpp>

namespace Cory::Framegraph {
SlotMapHandle TextureResourceManager::createTexture(std::string name,
                                                    glm::u32vec3 size,
                                                    PixelFormat format,
                                                    Layout layout)
{
    CO_CORE_DEBUG(
        "Creating '{}' of {}x{}x{} ({} {})", name, size.x, size.y, size.z, format, layout);
    auto handle =
        resources_.emplace(
            Texture{.name = std::move(name),
                    .size = size,
                    .format = format,
                    .currentLayout = layout,
                    .resource = {}});
    return handle;
}
Texture TextureResourceManager::allocate(std::string name,
                                         glm::u32vec3 size,
                                         PixelFormat format,
                                         Layout layout)
{
    CO_CORE_DEBUG(
        "Allocating '{}' of {}x{}x{} ({} {})", name, size.x, size.y, size.z, format, layout);
    auto handle =
        resources_.emplace(
            Texture{.name = std::move(name),
                .size = size,
                .format = format,
                .currentLayout = layout,
                .resource = {/*todo*/}});
    return resources_[handle];
}
}