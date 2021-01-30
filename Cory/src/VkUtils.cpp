#include "VkUtils.h"

#include "Context.h"

#include <fmt/format.h>

namespace Cory {

QueueFamilyIndices findQueueFamilies(const vk::PhysicalDevice &device,
                                     vk::SurfaceKHR surface)
{
  auto queueFamilies = device.getQueueFamilyProperties();

  QueueFamilyIndices indices;

  int i = 0;
  for (const auto &queueFamily : queueFamilies) {
    if (queueFamily.queueFlags & vk::QueueFlagBits::eGraphics) {
      indices.graphicsFamily = i;
    }
    if (queueFamily.queueFlags & vk::QueueFlagBits::eCompute) {
      indices.computeFamily = i;
    }
    if (queueFamily.queueFlags & vk::QueueFlagBits::eTransfer) {
      indices.transferFamily = i;
    }

    if (device.getSurfaceSupportKHR(i, surface)) {
      indices.presentFamily = i;
    }
    i++;
  }

  return indices;
}

uint32_t findMemoryType(vk::PhysicalDevice physicalDevice, uint32_t typeFilter,
                        vk::MemoryPropertyFlags properties)
{
  vk::PhysicalDeviceMemoryProperties memProperties =
      physicalDevice.getMemoryProperties();

  for (uint32_t i{}; i < memProperties.memoryTypeCount; ++i) {
    if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags &
                                    properties) == properties) {
      return i;
    }
  }

  throw std::runtime_error("Failed to find a suitable memory type!");
}

vk::Format findSupportedFormat(vk::PhysicalDevice physicalDevice,
                               const std::vector<vk::Format> &candidates,
                               vk::ImageTiling tiling,
                               vk::FormatFeatureFlags features)
{
  for (const vk::Format &format : candidates) {
    vk::FormatProperties props = physicalDevice.getFormatProperties(format);

    if (tiling == vk::ImageTiling::eLinear &&
        (props.linearTilingFeatures & features) == features) {
      return format;
    }
    if (tiling == vk::ImageTiling::eOptimal &&
        (props.optimalTilingFeatures & features) == features) {
      return format;
    }
  }
  throw std::runtime_error("failed to find supported format!");
}

vk::Format findDepthFormat(vk::PhysicalDevice physicalDevice)
{
  return findSupportedFormat(
      physicalDevice,
      {vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
      vk::ImageTiling::eOptimal,
      vk::FormatFeatureFlagBits::eDepthStencilAttachment);
}

bool hasStencilComponent(vk::Format format)
{
  return format == vk::Format::eD32SfloatS8Uint ||
         format == vk::Format::eD16UnormS8Uint ||
         format == vk::Format::eD24UnormS8Uint || format == vk::Format::eS8Uint;
}

vk::SampleCountFlagBits
getMaxUsableSampleCount(vk::PhysicalDevice physicalDevice)
{
  auto physicalDeviceProperties = physicalDevice.getProperties();

  vk::SampleCountFlags counts =
      physicalDeviceProperties.limits.framebufferColorSampleCounts &
      physicalDeviceProperties.limits.framebufferDepthSampleCounts;

  if (counts & vk::SampleCountFlagBits::e64)
    return vk::SampleCountFlagBits::e64;

  if (counts & vk::SampleCountFlagBits::e32)
    return vk::SampleCountFlagBits::e32;

  if (counts & vk::SampleCountFlagBits::e16)
    return vk::SampleCountFlagBits::e16;

  if (counts & vk::SampleCountFlagBits::e8)
    return vk::SampleCountFlagBits::e8;

  if (counts & vk::SampleCountFlagBits::e4)
    return vk::SampleCountFlagBits::e4;

  if (counts & vk::SampleCountFlagBits::e2)
    return vk::SampleCountFlagBits::e2;

  return vk::SampleCountFlagBits::e1;
}

Cory::SwapChainSupportDetails querySwapChainSupport(vk::PhysicalDevice device,
                                                    vk::SurfaceKHR surface)
{
  SwapChainSupportDetails details;
  details.capabilities = device.getSurfaceCapabilitiesKHR(surface);
  details.formats = device.getSurfaceFormatsKHR(surface);
  details.presentModes = device.getSurfacePresentModesKHR(surface);

  return details;
}

SingleTimeCommandBuffer::SingleTimeCommandBuffer(GraphicsContext &ctx)
    : m_ctx{ctx}
{
  vk::CommandBufferAllocateInfo allocInfo{};
  allocInfo.level = vk::CommandBufferLevel::ePrimary;
  allocInfo.commandPool = *m_ctx.transientCmdPool;
  allocInfo.commandBufferCount = 1;

  m_commandBuffer =
      std::move(m_ctx.device->allocateCommandBuffersUnique(allocInfo)[0]);

  vk::CommandBufferBeginInfo beginInfo{};

  m_commandBuffer->begin(beginInfo);
}

SingleTimeCommandBuffer::~SingleTimeCommandBuffer()
{
  m_commandBuffer->end();

  vk::SubmitInfo submitInfo{};
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &m_commandBuffer.get();

  m_ctx.graphicsQueue.submit({submitInfo}, vk::Fence{});
  m_ctx.graphicsQueue.waitIdle();
}

vk::Viewport VkDefaults::Viewport(vk::Extent2D swapChainExtent)
{
  return vk::Viewport{
      0.0f, /*x*/
      0.0f, /*x*/
      (float)swapChainExtent.width,
      (float)swapChainExtent.height,
      0.0f, /*minDepth*/
      1.0f  /*maxDepth*/
  };
}

vk::PipelineViewportStateCreateInfo
VkDefaults::ViewportState(vk::Viewport &viewport, vk::Rect2D &scissor)
{
  vk::PipelineViewportStateCreateInfo viewportState{};
  viewportState.viewportCount = 1;
  viewportState.pViewports = &viewport;
  viewportState.scissorCount = 1;
  viewportState.pScissors = &scissor;

  return viewportState;
}

vk::PipelineRasterizationStateCreateInfo VkDefaults::Rasterizer()
{
  vk::PipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.depthClampEnable =
      false; // depth clamp: depth is clamped for fragments instead
             // of discarding them. might be useful for shadow maps?
  rasterizer.rasterizerDiscardEnable =
      false; // completely disable rasterizer/framebuffer output
  rasterizer.polygonMode =
      vk::PolygonMode::eFill;  // _LINE and _POINT are alternatives, but
                               // require enabling GPU feature
  rasterizer.lineWidth = 1.0f; // >1.0 requires 'wideLines' GPU feature
  rasterizer.cullMode = vk::CullModeFlagBits::eBack;
  rasterizer.frontFace = vk::FrontFace::eCounterClockwise;
  rasterizer.depthBiasEnable = false;
  rasterizer.depthBiasConstantFactor = 0.0f;
  rasterizer.depthBiasClamp = 0.0f;
  rasterizer.depthBiasSlopeFactor = 0.0f;
  return rasterizer;
}

vk::PipelineMultisampleStateCreateInfo VkDefaults::Multisampling(
    vk::SampleCountFlagBits samples /*= vk::SampleCountFlagBits::e1*/)
{
  vk::PipelineMultisampleStateCreateInfo multisampling{};
  multisampling.sampleShadingEnable = false;
  multisampling.rasterizationSamples = samples;
  multisampling.minSampleShading = 0.2f; // controls how smooth the msaa
  multisampling.pSampleMask = nullptr;   // ?
  multisampling.alphaToCoverageEnable = false;
  multisampling.alphaToOneEnable = false;
  return multisampling;
}

vk::PipelineDepthStencilStateCreateInfo VkDefaults::DepthStencil()
{
  vk::PipelineDepthStencilStateCreateInfo depthStencil{};
  depthStencil.depthTestEnable = VK_TRUE;
  depthStencil.depthWriteEnable = VK_TRUE;
  depthStencil.depthCompareOp = vk::CompareOp::eLess;
  depthStencil.depthBoundsTestEnable = false;
  depthStencil.minDepthBounds = 0.0f;
  depthStencil.maxDepthBounds = 1.0f;
  depthStencil.stencilTestEnable = false;
  depthStencil.front = vk::StencilOpState{};
  depthStencil.back = vk::StencilOpState{};
  return depthStencil;
}

vk::PipelineColorBlendAttachmentState VkDefaults::AttachmentBlendDisabled()
{
  vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
  colorBlendAttachment.colorWriteMask =
      vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
      vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
  colorBlendAttachment.blendEnable = false;
  colorBlendAttachment.srcColorBlendFactor = vk::BlendFactor::eOne;
  colorBlendAttachment.dstColorBlendFactor = vk::BlendFactor::eZero;
  colorBlendAttachment.colorBlendOp = vk::BlendOp::eAdd;
  colorBlendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
  colorBlendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eZero;
  colorBlendAttachment.alphaBlendOp = vk::BlendOp::eAdd;

  return colorBlendAttachment;
}

vk::PipelineColorBlendStateCreateInfo VkDefaults::BlendState(
    std::vector<vk::PipelineColorBlendAttachmentState> *attachmentStages)
{
  // note: you can only do EITHER color blending per attachment, or logic
  // blending. enabling logic blending will override/disable the blend ops above
  vk::PipelineColorBlendStateCreateInfo colorBlending{};
  colorBlending.logicOpEnable = false;
  colorBlending.logicOp = vk::LogicOp::eCopy;
  if (!attachmentStages) {
    colorBlending.attachmentCount = 0;
    colorBlending.pAttachments = nullptr;
  }
  else {
    colorBlending.attachmentCount =
        static_cast<uint32_t>(attachmentStages->size());
    colorBlending.pAttachments = attachmentStages->data();
  }
  colorBlending.blendConstants[0] = 0.0f;
  colorBlending.blendConstants[1] = 0.0f;
  colorBlending.blendConstants[2] = 0.0f;
  colorBlending.blendConstants[3] = 0.0f;

  return colorBlending;
}

vk::PipelineLayoutCreateInfo
VkDefaults::PipelineLayout(vk::DescriptorSetLayout &layout)
{
  // stores/manages shader uniform values
  vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.setLayoutCount = 1;
  pipelineLayoutInfo.pSetLayouts = &layout;
  pipelineLayoutInfo.pushConstantRangeCount = 0;
  pipelineLayoutInfo.pPushConstantRanges = nullptr;

  return pipelineLayoutInfo;
}

} // namespace Cory