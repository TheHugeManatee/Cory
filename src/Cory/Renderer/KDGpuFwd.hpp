#pragma once

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

namespace KDGpuKDGui {
class View;
}

namespace KDGpuUtils {
class ResourceDeleter;
}