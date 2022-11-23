#pragma once

#include <Cory/Base/SlotMap.hpp>
#include <Cory/Framegraph/Common.hpp>

#include <glm/vec3.hpp>

#include <cstdint>
#include <string>

namespace Cory::Framegraph {

// handles the transient resources created/destroyed during a frame
class TextureResourceManager {
  public:
    TextureResourceManager(Context &ctx);

    TextureHandle declareTexture(TextureInfo info, Layout initialLayout);

    TextureHandle registerExternal(TextureInfo info,
                                   Layout layout,
                                   AccessFlags lastWriteAccess,
                                   AccessFlags lastWriteStage,
                                   Magnum::Vk::Image &resource);

    void allocate(const std::vector<TextureHandle> &handles)
    {
        for (const auto &handle : handles) {
            auto &r = textureResources_[handle];
            // don't allocate external resources or resources that are already allocated
            if (r.state.status != TextureMemoryStatus::Virtual) { continue; }

            allocate(handle);
        }
    }

    // emit a synchronization to sync subsequent reads with the last write access
    void readBarrier(Magnum::Vk::CommandBuffer &cmdBuffer,
                     TextureHandle handle,
                     TextureAccessInfo readAccessInfo);
    // emit a synchronization to sync a subsequent read/write with the last write, as well as with
    // future reads
    void writeBarrier(Magnum::Vk::CommandBuffer &cmdBuffer,
                      TextureHandle handle,
                      TextureAccessInfo writeAccessInfo);

    const TextureInfo &info(TextureHandle handle);
    Magnum::Vk::Image &image(TextureHandle handle);
    Magnum::Vk::ImageView &imageView(TextureHandle handle);

    void clear();

  private:
    void allocate(TextureHandle handle);
    std::unique_ptr<struct TextureManagerPrivate> data_;
};

} // namespace Cory::Framegraph