#pragma once
/**
 * @file forward declarations, common structures and enums for the RenderCore component
 */

#include <Cory/Base/Common.hpp> // for SlotMapHandle

namespace Cory {
// forward declared classes/structs
class Context;
class CpuBuffer;
class RenderManager;
class Shader;
class SingleShotCommandBuffer;
class Swapchain;

// enums
enum class ShaderType {
    eUnknown = 0,
    eVertex = 1 << 0,   // VK_SHADER_STAGE_VERTEX_BIT
    eGeometry = 1 << 3, // VK_SHADER_STAGE_GEOMETRY_BIT
    eFragment = 1 << 4, // VK_SHADER_STAGE_FRAGMENT_BIT
    eCompute = 1 << 5,  // VK_SHADER_STAGE_COMPUTE_BIT
};
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
enum class FenceCreateMode { Unsignaled, Signaled };

/// generic handle type to wrap slot map handles in a type-safe way
template <typename T> class ResourceHandle {
  public:
    /**
     * default constructor constructs an invalid handle! valid handles can
     * only be obtained from the ResourceManager!
     */
    ResourceHandle() = default;

  private:
    friend class ResourceManager;
    /* implicit */ ResourceHandle(SlotMapHandle handle)
        : handle_{handle}
    {
    }
    SlotMapHandle handle_{};
};
using ShaderHandle = ResourceHandle<Shader>;

} // namespace Cory

DECLARE_ENUM_BITFIELD(Cory::ShaderType);
DECLARE_ENUM_BITFIELD(Cory::DebugMessageType);

namespace Magnum::Vk {
class CommandPool;
class Device;
class Instance;
class Mesh;
class Pipeline;
class PipelineLayout;
class RenderPass;
} // namespace Magnum::Vk

// Vulkan forward definitions
struct VkDebugUtilsMessengerCallbackDataEXT;
using VkInstance = struct VkInstance_T *;
