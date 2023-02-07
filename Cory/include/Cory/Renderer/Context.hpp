#pragma once

#include <Cory/Base/Callback.hpp>
#include <Cory/Base/Common.hpp>
#include <Cory/Renderer/Common.hpp>
#include <Cory/Renderer/Semaphore.hpp>
#include <Cory/Renderer/VulkanUtils.hpp>

#include <Magnum/Vk/Fence.h>

#include <magic_enum.hpp>
#include <memory>
#include <string>
#include <string_view>

namespace Cory {

struct DebugMessageInfo {
    DebugMessageSeverity severity;
    DebugMessageType messageType;
    int32_t messageIdNumber;
    std::string message;
};

/**
 * The main context for cory (collects pretty much everything).
 */
class Context : NoCopy {
  public:
    Context();
    ~Context();

    // movable
    Context(Context &&rhs);
    Context &operator=(Context &&rhs);

    std::string name() const;

    [[nodiscard]] Semaphore createSemaphore(std::string_view name = "");
    [[nodiscard]] Magnum::Vk::Fence createFence(std::string_view name = "",
                                                FenceCreateMode mode = {});

    bool isHeadless() const;

    Magnum::Vk::Instance &instance();
    Magnum::Vk::DeviceProperties &physicalDevice();
    Magnum::Vk::Device &device();
    DescriptorSets &descriptorSets();
    Magnum::Vk::CommandPool &commandPool();

    Magnum::Vk::Queue &graphicsQueue();
    uint32_t graphicsQueueFamily() const;
    Magnum::Vk::Queue &computeQueue();
    uint32_t computeQueueFamily() const;

    ResourceManager &resources();
    const ResourceManager &resources() const;

    /// register a callback that gets called on vulkan validation messages etc.
    void onVulkanDebugMessageReceived(std::function<void(const DebugMessageInfo &)> callback);

    // get the default mesh layout. if empty is true, will return an empty layout with zero
    // attachments
    const Magnum::Vk::MeshLayout &defaultMeshLayout(bool empty = false) const;
    // non-const only to allow casting to VkPipelineLayout
    Magnum::Vk::PipelineLayout &defaultPipelineLayout();
    // non-const only to allow casting to VkDescriptorSetLayotu
    Magnum::Vk::DescriptorSetLayout &defaultDescriptorSetLayout();

  private:
    std::unique_ptr<struct ContextPrivate> data_;
    void setupDebugMessenger();
};
// static_assert(std::movable<Context>, "Context must be movable");

} // namespace Cory