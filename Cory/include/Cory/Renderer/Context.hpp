#pragma once


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

/**
 * The main context for cory
 */
class Context : NoCopy, NoMove {
  public:
    Context();
    ~Context();

    std::string getName() const;

    // receive and process a message from the vulkan debug utils - should not be called directly,
    // only exposed for REASONS
    void receiveDebugUtilsMessage(DebugMessageSeverity severity,
                                  DebugMessageType messageType,
                                  const VkDebugUtilsMessengerCallbackDataEXT *callbackData);

    [[nodiscard]] Semaphore createSemaphore(std::string_view name = "");
    [[nodiscard]] Magnum::Vk::Fence createFence(std::string_view name = "",
                                                FenceCreateMode mode = {});

    bool isHeadless() const;

    Magnum::Vk::Instance &instance();
    Magnum::Vk::DeviceProperties &physicalDevice();
    Magnum::Vk::Device &device();
    Magnum::Vk::DescriptorPool& descriptorPool();
    Magnum::Vk::CommandPool &commandPool();

    Magnum::Vk::Queue &graphicsQueue();
    uint32_t graphicsQueueFamily() const;
    Magnum::Vk::Queue &computeQueue();
    uint32_t computeQueueFamily() const;

    ResourceManager& resources();
    const ResourceManager& resources() const;

  private:
    std::unique_ptr<struct ContextPrivate> data_;
    void setupDebugMessenger();
};

} // namespace Cory