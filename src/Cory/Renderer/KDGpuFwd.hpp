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
struct AdapterProperties;
class Buffer;
class RenderPass;
class PipelineLayout;
class GraphicsPipeline;
class BindGroup;
class Pipeline;
struct Device;
struct Fence;
struct GpuSemaphore;
struct Instance;
struct Texture;
struct TextureView;
using GraphicsApi = VulkanGraphicsApi;
using ResourceManager = VulkanResourceManager;
struct VertexBufferLayout;
struct VertexOptions;

} // namespace KDGpu

namespace KDGpuKDGui {
class View;
}