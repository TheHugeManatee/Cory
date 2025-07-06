#pragma once
/**
 * @file forward declarations, common structures and enums for the Renderer component
 */

#include <Cory/Base/Common.hpp> // for SlotMapHandle

#include <Cory/Renderer/KDGpuFwd.hpp>
#include <Cory/Renderer/Semaphore.hpp> // Semaphore.hpp is a tiny header so it's ok
#include <Cory/Renderer/Synchronization.hpp>
#include <vulkan/vulkan.h>

#include <KDGpu/gpu_core.h>

#include <KDGpu/command_recorder.h>
#include <cstdint>

namespace Cory {
// forward declared classes/structs
class Context;
struct ContextCreationInfo;
class CpuBuffer;
class RenderManager;
class Shader;
class ResourceManager;
class SingleShotCommandRecorder;
// Swapchain.hpp
struct SwapchainSupportDetails;
struct FrameContext;
class Swapchain;
class UniformBufferObjectBase;
template <typename BufferStruct>
    requires std::is_trivial_v<BufferStruct>
class UniformBufferObject;
class DescriptorSets;

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
using BufferHandle = PrivateTypedHandle<KDGpu::VulkanBuffer, ResourceManager>;
using PipelineHandle = PrivateTypedHandle<KDGpu::VulkanPipeline, ResourceManager>;
using ImageHandle = PrivateTypedHandle<KDGpu::VulkanTexture, ResourceManager>;
using ImageViewHandle = PrivateTypedHandle<KDGpu::VulkanTextureView, ResourceManager>;
using SamplerHandle = PrivateTypedHandle<KDGpu::VulkanSampler, ResourceManager>;
using DescriptorSetLayoutHandle = PrivateTypedHandle<KDGpu::VulkanBindGroup, ResourceManager>;

struct FrameContext {
    uint32_t index{};                    ///< the current swapchain image index
    uint32_t swapchainImageIndex{};      ///< the current swapchain image index
    uint64_t frameNumber{};              ///< the (monotonically increasing) frame number
    bool shouldRecreateSwapchain{false}; ///< set when window has been resized
    const KDGpu::Texture *swapchainImage{};
    KDGpu::TextureView *swapchainImageView{};
    KDGpu::Texture *colorImage{};
    KDGpu::TextureView *colorImageView{};
    KDGpu::Texture *depthImage{};
    KDGpu::TextureView *depthImageView{};
    /// Fence to synchronize when the GPU has finished executing the commands associated with this
    /// frame, and its resources can be safely reused.
    KDGpu::Fence *inFlight{};
    /// Semaphore will be signaled when the swapchain image has been acquired (i.e.
    /// when presentation engine has finished with a preceding "present" call
    KDGpu::GpuSemaphore *acquired{};
    /// Semaphore will be signaled when all rendering commands have been executed on the GPU
    KDGpu::GpuSemaphore *rendered{};

    KDGpu::CommandRecorder commandBuffer;
};

} // namespace Cory

DECLARE_ENUM_BITFIELD(Cory::ShaderType);
DECLARE_ENUM_BITFIELD(Cory::DebugMessageType);
DECLARE_ENUM_BITFIELD(Cory::BufferUsageBits);
DECLARE_ENUM_BITFIELD(Cory::MemoryFlagBits);