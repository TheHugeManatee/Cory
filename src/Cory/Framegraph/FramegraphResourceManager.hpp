#pragma once

#include <Cory/Base/SlotMap.hpp>
#include <Cory/Framegraph/Common.hpp>

#include <cstdint>

namespace Cory {

struct FramegraphBufferView {
    BufferDeviceAddress deviceAddress;
    Gpu::DeviceSize offset;
    Gpu::DeviceSize size;
    std::byte *cpu;
    bool hostVisible;
};

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
 *  - For textures, currently always creates an Image and corresponding ImageView, even
 *    though technically creating an ImageView and sampler could be avoided
 *    by having the knowledge from the framegraph how the texture will be used
 *  - Currently, allocates each Image/Buffer separately - technically, could use a GPU arena
 */
class FramegraphResourceManager : NoCopy {
  public:
    explicit FramegraphResourceManager(Context &ctx);
    ~FramegraphResourceManager();

    explicit FramegraphResourceManager(FramegraphResourceManager &&) noexcept;
    FramegraphResourceManager &operator=(FramegraphResourceManager &&) noexcept;
    void setContext(Context &ctx);

    void setCurrentFrameNumber(uint64_t frameNumber);
    [[nodiscard]] uint64_t currentFrameNumber() const;

    // Declare a new texture - will only create the metadata, not allocate the actual resource
    FramegraphTextureHandle declareTexture(TextureInfo info);

    // Adopt an external texture (e.g. a swapchain image) into the framegraph - will participate
    // in synchronization, but will not be destroyed by the framegraph
    FramegraphTextureHandle registerExternal(TextureInfo info,
                                             Sync::AccessType lastWriteAccess,
                                             Gpu::TextureHandle resource,
                                             Gpu::TextureViewHandle resourceView);

    void allocate(const std::vector<FramegraphTextureHandle> &handles);
    void extendUsage(FramegraphTextureHandle handle, Gpu::TextureUsageFlags usage);

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

    // Declare a new buffer - will only create the metadata, not allocate the actual resource
    FramegraphBufferHandle declareBuffer(BufferInfo info);

    // Adopt an external buffer into the framegraph - will participate in synchronization, but
    // will not be destroyed by the framegraph
    FramegraphBufferHandle
    registerExternal(BufferInfo info, Sync::AccessType lastWriteAccess, Gpu::BufferHandle resource);

    void allocate(const std::vector<FramegraphBufferHandle> &handles);
    void extendUsage(FramegraphBufferHandle handle, Gpu::BufferUsageFlags usage);

    /**
     * @brief create a synchronization barrier object to sync subsequent reads
     * @param handle the handle to synchronize
     * @param access the access type
     *
     * Will store the given @a access to sync subsequent accesses to the buffer
     */
    Sync::BufferBarrier synchronizeBuffer(FramegraphBufferHandle handle, Sync::AccessType access);

    [[nodiscard]] const BufferInfo &info(FramegraphBufferHandle handle) const;
    [[nodiscard]] FramegraphBufferView bufferView(FramegraphBufferHandle handle) const;
    [[nodiscard]] BufferDeviceAddress deviceAddress(FramegraphBufferHandle handle) const;
    [[nodiscard]] BufferState state(FramegraphBufferHandle handle) const;

    void clearFrame(uint64_t frameNumber);
    void clearAll();

  private:
    void allocate(FramegraphTextureHandle handle);
    std::unique_ptr<struct FramegraphResourceManagerPrivate> data_;
};

} // namespace Cory
