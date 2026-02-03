#pragma once

#include <Cory/Renderer/Common.hpp>
#include <Cory/Renderer/Gpu.hpp>

#include <KDGpu/bind_group_layout_options.h>

#include <memory>

namespace Cory {

enum class DescriptorSetType : uint32_t {
    GlobalData = 0,
    BindlessTextures = 1,
};

struct ImageBindPoint {
    uint32_t value;
        ImageBindPoint(uint32_t v)
            : value(v)
        {
        }
    constexpr operator uint32_t() const noexcept { return value; }
    enum Type : uint32_t {
        Texture2D = 0,
        Texture3D = 1,
        StorageImage2D = 2,
        StorageImage3D = 3,
        StorageImage2DMS = 5,
        Samplers = 4,
    };
};

struct DescriptorSetOptions {
    std::string label;
    Gpu::ShaderStageFlags shaderStages{Gpu::ShaderStageFlagBits::All};
};

/**
 * @brief Manages a global descriptor set for each frame in flight
 *
 * Descriptor sets are set up in a bindless fashion, with sets grouping different types:
 *   - Set 0: General UBOs filled/updated by the engine
 *       - binding 0: General UBO for global per-frame data managed by the engine,
 *                    e.g. frame index, time etc.
 *   - Set 1: Bindless Texture arrays (MAX_IMAGES entries each):
 *      - binding 0: samplers
 *      - binding 1: 2D combined image samplers
 *      - binding 2: 3D combined image samplers
 *      - binding 3: Read-write storage images (2D)
 *      - binding 4: Read-write storage images (3D)
 *
 * General usage idea is:
 *   - Initialization allocates and initializes the descriptor sets for each frame in flight
 *   - Each frame, before recording command buffers, the relevant entries are updated via
 *     write(). These writes are queued locally.
 *   - flush() applies all queued descriptor writes to the underlying bind groups (calls
 *     vkUpdateDescriptorSets)
 *   - The pipeline binds the relevant descriptor sets before drawing/dispatching
 *
 */
class DescriptorSets : NoCopy {
  public:
    static constexpr size_t MAX_IMAGES = 1024;
    static constexpr size_t MAX_SAMPLERS = 1024;

    /// by default constructs an uninitialized object - needs an init() call to initialize!
    DescriptorSets();

    ~DescriptorSets();

    /**
     * Initialize the descriptor set manager
     * @param device            the device to use
     * @param options     the options for the descriptor sets
     */
    void init(Gpu::Device &device, DescriptorSetOptions options);

    [[nodiscard]] const std::vector<Gpu::BindGroupLayoutHandle> &layouts() const noexcept;

    /**
     * Record a descriptor write for updating an image and potentially view and sampler.
     * Depending on the bind point, the additional parameters may not be required.
     */
    DescriptorSets &write(ImageBindPoint bindPoint,
                          gsl::index instanceIndex,
                          TextureHeapIndex textureIndex,
                          Gpu::TextureLayout layout,
                          Gpu::TextureViewHandle image = {},
                          Gpu::TextureSamplerHandle sampler = {});

    /// Write just an image sampler to the specified sampler index
    DescriptorSets &write(gsl::index instanceIndex,
                          SamplerHeapIndex samplerIndex,
                          Gpu::TextureSamplerHandle sampler);

    /// Access the bind group for the given set type and instance index
    [[nodiscard]] Gpu::BindGroup &get(DescriptorSetType type, gsl::index instanceIndex);

    /// bind the given instance index
    DescriptorSets &bind(Gpu::RenderPassCommandRecorder &cmd, gsl::index instanceIndex);
    DescriptorSets &bind(Gpu::RenderPassCommandRecorder &cmd,
                         gsl::index instanceIndex,
                         Gpu::PipelineLayoutHandle pipelineLayout);
    DescriptorSets &bind(Gpu::ComputePassCommandRecorder &cmd, gsl::index instanceIndex);

    /// Apply all queued descriptor writes
    DescriptorSets &flush(gsl::index instanceIndex);

  private:
    std::unique_ptr<struct DescriptorSetManagerPrivate> data_;
}; // namespace Cory

} // namespace Cory
