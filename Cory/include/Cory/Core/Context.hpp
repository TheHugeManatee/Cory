#pragma once

#include <memory>
#include <string>

// Vulkan forward definitions
struct VkDebugUtilsMessengerCallbackDataEXT;

namespace Cory {

enum class DebugMessageSeverity {
    Verbose = 0x00000001,
    Info = 0x00000010,
    Warning = 0x00000100,
    Error = 0x00001000,
};

enum class DebugMessageTypeBits {
    General = 0x00000001,
    Validation = 0x00000002,
    Performance = 0x00000004,
    DeviceAddressBinding = 0x00000008, ///< Provided by VK_EXT_device_address_binding_report
};

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
                                  DebugMessageTypeBits messageType,
                                  const VkDebugUtilsMessengerCallbackDataEXT *callbackData);

  private:
    std::unique_ptr<struct ContextPrivate> data;
};

} // namespace Cory
