#pragma once

#include <Cory/Renderer/Common.hpp>
#include <Cory/Renderer/Gpu.hpp>

#include <KDGpu/bind_group_layout_options.h>

#include <memory>

namespace Cory {

/**
 * Manages descriptor sets in a frequency-based manner
 *
 * Manages the first three available descriptor sets (0, 1, 2), while the fourth (and any
 * additional ones supported by the architecture) are left to the implementation to use as needed.
 * Implements roughly a Frequency-based descriptor model (with some slot-based ideas) as described
 * here:
 *
 * https://zeux.io/2020/02/27/writing-an-efficient-vulkan-renderer/#frequency-based-descriptor-sets
 *
 * A bindless design would be relatively complicated for us here because we do not have a fixed
 * material model.
 *
 * Consistently uses the bind points defined in @b DescriptorSets::BindPoints to bind the different
 * object types.
 */
class DescriptorSets {
  public:
    enum class SetType : uint32_t {
        /// data that updates only occasionally based e.g. on user input, static textures
        Static = 0,
        /// data that updates per-frame, e.g. time, material textures, camera matrix
        Frame = 1,
        /// per-pass resources like bound images, parameters etc.
        Pass = 2,
        /// free for user-defined usage
        User = 3,
    };

    enum class BindPoints : uint32_t {
        UniformBufferObject = 0,
        CombinedImageSampler = 1,
        StorageBuffer = 2
    };

    /// by default constructs an uninitialized object - needs an init() call to initialize!
    DescriptorSets();

    ~DescriptorSets();

    /**
     * Initialize the descriptor set manager
     * @param device            the device to use
     * @param defaultLayout     the layout to use for the three sets
     */
    void init(Gpu::Device &device, Gpu::BindGroupLayoutOptions defaultLayout);

    [[nodiscard]] const std::vector<Gpu::BindGroupLayoutHandle> &layouts() const noexcept;

    /**
     * Record a descriptor write for updating an UBO reference
     * @param type
     * @param frameInFlightIndex
     * @param ubo
     *
     * @note This write will not be issued until @b flushWrites() is called.
     */
    DescriptorSets &
    write(SetType type, gsl::index frameInFlightIndex, const UniformBufferObjectBase &ubo);

    /**
     * Record a descriptor write for updating image references
     *
     * @note This write will not be issued until @b flushWrites() is called.
     */
    DescriptorSets &write(SetType type,
                          gsl::index frameInFlightIndex,
                          gsl::span<Gpu::TextureLayout> layouts,
                          gsl::span<Gpu::TextureViewHandle> images,
                          gsl::span<Gpu::TextureSamplerHandle> samplers);
    DescriptorSets &write(SetType type,
                          gsl::index frameInFlightIndex,
                          const Gpu::Buffer &buffer);
    DescriptorSets &write(SetType type,
                          gsl::index frameInFlightIndex,
                          uint32_t binding,
                          const Gpu::Buffer &buffer);

    /**
     * @brief flush all updates, calling vkUpdateDescriptorSets with the previously recorded
     writes
     */
    DescriptorSets &flushWrites();

    [[nodiscard]] Gpu::BindGroup &get(SetType type, gsl::index instanceIndex);

    /// bind the given instance index
    DescriptorSets &bind(Gpu::RenderPassCommandRecorder &cmd, gsl::index instanceIndex);
    DescriptorSets &bind(Gpu::ComputePassCommandRecorder &cmd, gsl::index instanceIndex);

  private:
    std::unique_ptr<struct DescriptorSetManagerPrivate> data_;
};

} // namespace Cory
