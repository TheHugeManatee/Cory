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
} // namespace Cory
