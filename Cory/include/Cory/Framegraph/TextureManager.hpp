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
    TextureResourceManager(Context &ctx);
    ~TextureResourceManager();

    TextureHandle declareTexture(TextureInfo info);

    TextureHandle registerExternal(TextureInfo info,
                                   Layout layout,
                                   AccessFlags lastWriteAccess,
                                   PipelineStages lastWriteStage,
                                   Magnum::Vk::Image &resource);

    void allocate(const std::vector<TextureHandle> &handles);

    // emit a synchronization to sync subsequent reads with the last write access
    void readBarrier(Magnum::Vk::CommandBuffer &cmdBuffer,
                     TextureHandle handle,
                     TextureAccessInfo readAccessInfo);
    // record a write access to synchronize subsequent read accesses
    void recordWrite(Magnum::Vk::CommandBuffer &cmdBuffer,
                     TextureHandle handle,
                     TextureAccessInfo writeAccessInfo);

    void readWriteBarrier(Magnum::Vk::CommandBuffer &cmdBuffer,
                          TextureHandle handle,
                          TextureAccessInfo readAccessInfo,
                          TextureAccessInfo writeAccessInfo);

    const TextureInfo &info(TextureHandle handle);
    Magnum::Vk::Image &image(TextureHandle handle);
    Magnum::Vk::ImageView &imageView(TextureHandle handle);
    TextureState state(TextureHandle handle) const;

    void clear();

  private:
    void allocate(TextureHandle handle);
    std::unique_ptr<struct TextureManagerPrivate> data_;
};

} // namespace Cory::Framegraph