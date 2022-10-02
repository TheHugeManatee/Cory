#pragma once

#include <magic_enum.hpp>
#include <memory>
#include <string>

#include <Cory/Core/Common.hpp>

namespace Magnum::Vk {
class Device;
class Instance;
class CommandPool;
} // namespace Magnum::Vk

// Vulkan forward definitions
struct VkDebugUtilsMessengerCallbackDataEXT;
using VkInstance = struct VkInstance_T *;

namespace Cory {

enum class DebugMessageSeverity {
    Verbose = 0x00000001,
    Info = 0x00000010,
    Warning = 0x00000100,
    Error = 0x00001000,
};

enum class DebugMessageType {
    General = 0x00000001,
    Validation = 0x00000002,
    Performance = 0x00000004,
    DeviceAddressBinding = 0x00000008, ///< Provided by VK_EXT_device_address_binding_report
};
} // namespace Cory
DECLARE_ENUM_BITFIELD(Cory::DebugMessageType);

namespace Cory {

/**
 * The main context for cory
 */
class Context {
  public:
    Context();
    ~Context();

    std::string getName() const;

    // receive and process a message from the vulkan debug utils - should not be called directly,
    // only exposed for REASONS
    void receiveDebugUtilsMessage(DebugMessageSeverity severity,
                                  DebugMessageType messageType,
                                  const VkDebugUtilsMessengerCallbackDataEXT *callbackData);

    bool isHeadless() const;

    Magnum::Vk::Instance &instance();
    Magnum::Vk::Device &device();
    Magnum::Vk::CommandPool &commandPool();

  private:
    std::unique_ptr<struct ContextPrivate> data;
    void setupDebugMessenger();
};

} // namespace Cory