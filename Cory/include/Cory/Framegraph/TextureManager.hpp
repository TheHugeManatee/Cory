#pragma once

#include <Cory/Base/SlotMap.hpp>
#include <Cory/Framegraph/Common.hpp>

#include <glm/vec3.hpp>

#include <cstdint>
#include <string>

namespace Cory::Framegraph {

// handles the transient resources created/destroyed during a frame - tightly coupled with the
// Framegraph and Builder classes, not intended to be used directly
class TextureResourceManager {
  public:
    explicit TextureResourceManager(Context &ctx);
    ~TextureResourceManager();

    TextureHandle declareTexture(TextureInfo info);

    TextureHandle registerExternal(TextureInfo info,
                                   Sync::AccessType lastWriteAccess,
                                   Magnum::Vk::Image &resource,
                                   Magnum::Vk::ImageView &resourceView);

    void allocate(const std::vector<TextureHandle> &handles);

    // emit a synchronization to sync subsequent reads with the last write access. will store the new
    Sync::ImageBarrier synchronizeTexture(TextureHandle handle,
                                          Sync::AccessType access,
                                          ImageContents contentsMode);

    [[nodiscard]] const TextureInfo &info(TextureHandle handle) const;
    Magnum::Vk::Image &image(TextureHandle handle);
    Magnum::Vk::ImageView &imageView(TextureHandle handle);
    [[nodiscard]] TextureState state(TextureHandle handle) const;

    void clear();

  private:
    void allocate(TextureHandle handle);
    std::unique_ptr<struct TextureManagerPrivate> data_;
};

} // namespace Cory::Framegraph