#pragma once

#include <Cory/Base/SlotMap.hpp>
#include <Cory/Framegraph/Common.hpp>

namespace Cory {

/**
 * @brief handles the transient resources created/destroyed during a frame
 *
 * This class is tightly coupled with the @a Framegraph and @a RenderTaskBuilder, not
 * intended to be used directly.
 *
 * It is intended to capture all transient resources for one frame, and is expected to be cleared
 * fully after the frame has been rendered.
 *
 * Implementation Notes:
 *  - Currently always creates an Image and corresponding ImageView, even
 *    though technically creating an ImageView and sampler could be avoided
 *    by having the knowledge from the framegraph how the texture will be used
 *  - Currently, allocates each Image separately - technically, could use an
 *    GPU arena for this
 */
class TextureManager : NoCopy {
  public:
    explicit TextureManager(Context &ctx);
    ~TextureManager();

    explicit TextureManager(TextureManager &&) noexcept;
    TextureManager &operator=(TextureManager &&) noexcept;

    // Declare a new texture - will only create the metadata, not allocate the actual resource
    FramegraphTextureHandle declareTexture(TextureInfo info);

    // Adopt an external texture (e.g. a swapchain image) into the framegraph - will participate
    // in synchronization, but will not be destroyed by the framegraph
    FramegraphTextureHandle registerExternal(TextureInfo info,
                                             Sync::AccessType lastWriteAccess,
                                             Gpu::TextureHandle &resource,
                                             Gpu::TextureViewHandle &resourceView);

    void allocate(const std::vector<FramegraphTextureHandle> &handles);

    /**
     * @brief create a synchronization barrier object to sync subsequent reads
     * @param handle the handle to synchronize
     * @param access the access type
     * @param contentsMode whether the previous contents should be retained or discarded when
     *        accessing the texture - choose ImageContents::Discard if you overwrite the contents
     *
     * Will store the given @a access to sync subsequent accesses to the texture
     */
    Sync::ImageBarrier synchronizeTexture(FramegraphTextureHandle handle,
                                          Sync::AccessType access,
                                          ImageContents contentsMode);

    [[nodiscard]] const TextureInfo &info(FramegraphTextureHandle handle) const;
    [[nodiscard]] Gpu::TextureHandle image(FramegraphTextureHandle handle) const;
    [[nodiscard]] Gpu::TextureViewHandle imageView(FramegraphTextureHandle handle) const;
    [[nodiscard]] TextureState state(FramegraphTextureHandle handle) const;

    void clear();

  private:
    void allocate(FramegraphTextureHandle handle);
    std::unique_ptr<struct TextureManagerPrivate> data_;
};

} // namespace Cory