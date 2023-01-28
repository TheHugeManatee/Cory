#pragma once

#include <Cory/Renderer/Common.hpp>

#include <cstdint>
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
 */
class DescriptorSetManager {
  public:
    enum class SetType {
        /// data that updates only occasionally based e.g. on user input, static textures
        Static = 0,
        /// data that updates per-frame, e.g. time, material textures, camera matrix
        Frame = 1,
        /// per-pass resources like bound images, parameters etc.
        Pass = 2,
        /// free for user-defined usage
        User = 3,
    };

    /// by default constructs an uninitialized object - needs an init() call to initialize!
    DescriptorSetManager();

    ~DescriptorSetManager();

    /**
     * Initialize the descriptor set manager
     * @param device            the device for which to allocate the layouts
     * @param resourceManager   the resource manager to use for allocating resources
     * @param defaultLayout     the layout to use for the three sets
     * @param instances         number of instances for each descriptor set.
     *
     * @a instances is usually equal to the number of frames in flight.
     */
    void init(Magnum::Vk::Device &device,
              ResourceManager &resourceManager,
              Magnum::Vk::DescriptorSetLayoutCreateInfo defaultLayout,
              uint32_t instances);

    /**
     * flush all updates, calling vkUpdateDescriptorSets as appropriate
     * @param device        the device
     * @param modifiedType  the lowest modified type
     * @param instanceIndex the index of the modified set instances
     * This will update all descriptor sets "higher" than the @a modifiedType
     */
    void update(Magnum::Vk::Device &device, SetType modifiedType, gsl::index instanceIndex);

    [[nodiscard]] DescriptorSetLayoutHandle layout();

    /// the number of instances available
    [[nodiscard]] uint32_t instances() const;

    [[nodiscard]] Magnum::Vk::DescriptorSet& get(SetType type, gsl::index instanceIndex);

  private:
    std::unique_ptr<struct DescriptorSetManagerPrivate> data_;
};

} // namespace Cory