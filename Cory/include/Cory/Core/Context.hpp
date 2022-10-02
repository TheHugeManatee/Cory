#pragma once

#include <magic_enum.hpp>
#include <memory>
#include <string>

#include <Magnum/Vk/Device.h>
#include <Magnum/Vk/Instance.h>

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
template <> struct magic_enum::customize::enum_range<Cory::DebugMessageType> {
    static constexpr bool is_flags = true;
};

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

    Magnum::Vk::Instance &instance() const;
    Magnum::Vk::Device &device() const;

  private:
    std::unique_ptr<struct ContextPrivate> data;
    void setupDebugMessenger();
};

} // namespace Cory