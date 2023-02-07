#pragma once
/**
 * @file forward declarations, common structures and enums for the Renderer component
 */

#include <Cory/Base/Common.hpp> // for SlotMapHandle

#include <Corrade/Containers/StringStlView.h>
#include <Magnum/Vk/Vk.h> // forward declaration header
#include <Magnum/Vk/Vulkan.h>

#include <Cory/Renderer/Synchronization.hpp>
#include <Cory/Renderer/flextVkExt.h> // extensions

#include <cstdint>

namespace Cory {
// forward declared classes/structs
class Context;
class CpuBuffer;
class RenderManager;
class Shader;
class ResourceManager;
class SingleShotCommandBuffer;
// Swapchain.hpp
struct SwapchainSupportDetails;
struct FrameContext;
class Swapchain;
class UniformBufferObjectBase;
template <typename BufferStruct>
    requires std::is_trivial_v<BufferStruct>
class UniformBufferObject;
class DescriptorSets;

using PixelFormat = Magnum::Vk::PixelFormat;
bool isColorFormat(PixelFormat format);
bool isDepthFormat(PixelFormat format);
bool isStencilFormat(PixelFormat format);

// enums
enum class ShaderType : uint32_t {
    eUnknown = 0,
    eVertex = VK_SHADER_STAGE_VERTEX_BIT,
    eGeometry = VK_SHADER_STAGE_GEOMETRY_BIT,
    eFragment = VK_SHADER_STAGE_FRAGMENT_BIT,
    eCompute = VK_SHADER_STAGE_COMPUTE_BIT,
};
enum class DebugMessageSeverity : uint32_t {
    Verbose = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
    Info = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
    Warning = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
    Error = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
};
enum class DebugMessageType : uint32_t {
    General = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT,
    Validation = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
    Performance = VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
};
enum class FenceCreateMode { Unsignaled, Signaled };
enum class BufferUsageBits : uint32_t {
    TransferSource = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    TransferDestination = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    UniformTexelBuffer = VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT,
    StorageTexelBuffer = VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT,
    UniformBuffer = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
    StorageBuffer = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
    IndexBuffer = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
    VertexBuffer = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
    IndirectBuffer = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
    ShaderBindingTable = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR,
    AccelerationStructureBuildInputReadOnly =
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
    AccelerationStructureStorage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR
};
using BufferUsage = BitField<BufferUsageBits>;
enum class MemoryFlagBits : uint32_t {
    DeviceLocal = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    HostVisible = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
    HostCoherent = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    HostCached = VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
    LazilyAllocated = VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT,
};
using MemoryFlags = BitField<MemoryFlagBits>;

using ShaderHandle = PrivateTypedHandle<Shader, ResourceManager>;
static_assert(std::movable<ShaderHandle> && std::copyable<ShaderHandle>);
using BufferHandle = PrivateTypedHandle<Magnum::Vk::Buffer, ResourceManager>;
using PipelineHandle = PrivateTypedHandle<Magnum::Vk::Pipeline, ResourceManager>;
using SamplerHandle = PrivateTypedHandle<Magnum::Vk::Sampler, ResourceManager>;
using DescriptorSetLayoutHandle =
    PrivateTypedHandle<Magnum::Vk::DescriptorSetLayout, ResourceManager>;

} // namespace Cory

DECLARE_ENUM_BITFIELD(Cory::ShaderType);
DECLARE_ENUM_BITFIELD(Cory::DebugMessageType);
DECLARE_ENUM_BITFIELD(Cory::BufferUsageBits);
DECLARE_ENUM_BITFIELD(Cory::MemoryFlagBits);