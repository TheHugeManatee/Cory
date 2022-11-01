#pragma once

#include <Cory/Base/SlotMap.hpp>

#include <glm/vec3.hpp>

#include <cstdint>
#include <string>

namespace Cory::Framegraph {

using PlaceholderT = uint64_t;

enum class PixelFormat { D32, RGBA32 };
enum class Layout {
    Undefined,
    ColorAttachment,
    DepthStencilAttachment,
    TransferSource,
    PresentSource
};
enum class ResourceState { Clear, DontCare, Keep };

using RenderPassHandle = std::string;

using ResourceHandle = SlotMapHandle;

struct Texture {
    std::string name;
    glm::u32vec3 size;
    PixelFormat format;
    Layout currentLayout;
    PlaceholderT resource; // the Vk::Image
};

struct TextureHandle {
    std::string name;
    glm::u32vec3 size;
    PixelFormat format;
    Layout layout;
    ResourceHandle rsrcHandle; // handle to be used to reference the texture
};

struct MutableTextureHandle {
    std::string name;
    glm::u32vec3 size;
    PixelFormat format;
    Layout layout;
    ResourceHandle rsrcHandle;

    /*implicit*/ operator TextureHandle() const
    {
        return {.name = name,
                .size = size,
                .format = format,
                .layout = layout,
                .rsrcHandle = rsrcHandle};
    }
};

// handles the transient resources created/destroyed during a frame
class TextureResourceManager {
  public:
    ResourceHandle
    createTexture(std::string name, glm::u32vec3 size, PixelFormat format, Layout layout)
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
        return handle;
    }

    // access a texture via its handle
    const Texture &operator[](TextureHandle textureHandle)
    {
        return resources_[textureHandle.rsrcHandle];
    }

    // allocate backing memory and resolve memory aliasing for all previously created resources
    void allocateAll() {
        // allocate backing memory and assign the .resource member for all resources
    }

  private:
    Texture allocate(std::string name, glm::u32vec3 size, PixelFormat format, Layout layout)
    {
        CO_CORE_DEBUG(
            "Allocating '{}' of {}x{}x{} ({} {})", name, size.x, size.y, size.z, format, layout);
        auto handle =
        resources_.emplace(
                           Texture{.name = std::move(name),
                                   .size = size,
                                   .format = format,
                                   .currentLayout = layout,
                                   .resource = {}});
        return resources_[handle];
    }

    ResourceHandle nextHandle_{};
    SlotMap<Texture> resources_;
};
} // namespace Cory::Framegraph