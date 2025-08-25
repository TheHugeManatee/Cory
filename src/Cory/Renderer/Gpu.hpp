#pragma once

#include <KDGpu/gpu_core.h>

namespace KDGpu {

class VulkanGraphicsApi;
class VulkanResourceManager;
struct VulkanBindGroup;
struct VulkanBindGroupLayout;
struct VulkanBuffer;
struct VulkanCommandBuffer;
struct VulkanCommandRecorder;
struct VulkanFence;
struct VulkanInstance;
struct VulkanPipeline;
struct VulkanPipelineLayout;
struct VulkanQueue;
struct VulkanSampler;
struct VulkanTexture;
struct VulkanTextureView;

class Adapter;
class BindGroup;
class Buffer;
class CommandRecorder;
class GraphicsPipeline;
class Pipeline;
class PipelineLayout;
class RenderPass;
class RenderPassCommandRecorder;
class Surface;
class Swapchain;
struct AdapterProperties;
struct Device;
struct Fence;
struct GpuSemaphore;
struct Instance;
struct Texture;
struct TextureView;
struct VertexBufferLayout;
struct VertexOptions;
using GraphicsApi = VulkanGraphicsApi;
using ResourceManager = VulkanResourceManager;

template <typename T> class Handle;
// typedefs for the various handle types used by the VulkanResourceManager
using DeviceHandle = Handle<struct Device_t>;
using TextureHandle = Handle<struct Texture_t>;
using TextureViewHandle = Handle<struct TextureView_t>;
using BufferHandle = Handle<struct Buffer_t>;
using CommandBufferHandle = Handle<struct CommandBuffer_t>;
using CommandRecorderHandle = Handle<struct CommandRecorder_t>;
using GraphicsPipelineHandle = Handle<struct Pipeline_t>;

} // namespace KDGpu

namespace KDGpuUtils {
class ResourceDeleter;
}

namespace Gpu = KDGpu;
// import a few short-hands directly into the Cory namespace
namespace Cory {
using CommandRecorder = Gpu::CommandRecorder;
using Device = Gpu::Device;
using Fence = Gpu::Fence;
using GpuSemaphore = Gpu::GpuSemaphore;
using Surface = Gpu::Surface;
using Texture = Gpu::Texture;
using TextureView = Gpu::TextureView;
using ResourceDeleter = KDGpuUtils::ResourceDeleter;

using TextureFormat = Gpu::Format;

[[nodiscard]] Gpu::TextureAspectFlags flagsForFormat(TextureFormat format);

} // namespace Cory
