#pragma once

#include <Cory/Base/SlotMap.hpp>
#include <Cory/Framegraph/Common.hpp>

#include <glm/vec3.hpp>

#include <cstdint>
#include <string>

namespace Cory {

/**
 * @brief handles the transient resources created/destroyed during a frame
 *
 * This class is tightly coupled with the @a Framegraph and @a Builder, not
 * intended to be used directly.
 *
 * Implementation Notes:
 *  - Currently always creates an Image and corresponding ImageView, even
 *    though technically creating an ImageView and sampler could be avoided
 *    by having the knowledge from the framegraph how the texture will be used
 *  - Currently, allocates each Image separately - technically, could use an
 *    GPU arena for this
 */
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

    /**
     * @brief create a synchronization barrier object to sync subsequent reads
     * @param handle the handle to synchronize
     * @param access the access type
     * @param contentsMode whether the previous contents should be retained or discarded when
     *        accessing the texture - choose ImageContents::Discard if you overwrite the contents
     *
     * Will store the given @a access to sync subsequent accesses to the texture
     */
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

} // namespace Cory