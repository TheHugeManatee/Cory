#pragma once

#include <Cory/Base/SlotMap.hpp>

#include <glm/vec3.hpp>

#include <cstdint>
#include <string>

namespace Cory::Framegraph {

using PlaceholderT = uint64_t;

enum class PixelFormat { D32, RGBA32 };
enum class Layout { Undefined, Color, DepthStencil, TransferSource, PresentSource };
enum class ResourceState { Clear, DontCare, Keep };
using ResourceHandle = SlotMapHandle;

struct Texture {
    std::string name;
    glm::u32vec3 size;
    PixelFormat format;
    Layout currentLayout;
    bool external{false};
    PlaceholderT resource; // the Vk::Image
};

struct TextureHandle {
    std::string name;
    glm::u32vec3 size;
    PixelFormat format;
    Layout layout;
    bool external{false};
    SlotMapHandle rsrcHandle; // handle to be used to reference the texture
};

struct MutableTextureHandle {
    std::string name;
    glm::u32vec3 size;
    PixelFormat format;
    Layout layout;
    bool external{false};
    SlotMapHandle rsrcHandle;

    /*implicit*/ operator TextureHandle() const
    {
        return {.name = name,
                .size = size,
                .format = format,
                .layout = layout,
                .external = external,
                .rsrcHandle = rsrcHandle};
    }
};

// handles the transient resources created/destroyed during a frame
class TextureResourceManager {
  public:
    SlotMapHandle
    createTexture(std::string name, glm::u32vec3 size, PixelFormat format, Layout layout);

    SlotMapHandle registerExternal(Texture externalTexture)
    {
        externalTexture.external = true;
        return resources_.emplace(std::move(externalTexture));
    }

    // access a texture via its handle
    const Texture &operator[](TextureHandle textureHandle) const
    {
        return resources_[textureHandle.rsrcHandle];
    }

    // allocate backing memory and resolve memory aliasing for all previously created resources
    void allocateAll()
    {
        // allocate backing memory and assign the .resource member for all resources
    }

  private:
    Texture allocate(std::string name, glm::u32vec3 size, PixelFormat format, Layout layout);

    SlotMap<Texture> resources_;
};

} // namespace Cory::Framegraph