#pragma once

#include <memory>
#include <vulkan/vulkan.hpp>

struct GLFWwindow;

namespace Cory {
class GraphicsContext;
class SwapChain;

class ImGuiLayer {
public:
  ImGuiLayer();

  void init(GLFWwindow *window, GraphicsContext &ctx,
            vk::SampleCountFlagBits msaaSamples, vk::ImageView renderedImage,
            SwapChain &swapChain);

  void deinit(GraphicsContext &ctx);

  void newFrame(GraphicsContext &ctx);
  void drawFrame(GraphicsContext &ctx, uint32_t currentFrameIdx);

private:
  void createImguiDescriptorPool(GraphicsContext &ctx);

  void createImguiRenderpass(vk::Format format,
                             vk::SampleCountFlagBits msaaSamples,
                             GraphicsContext &ctx);

private:
  std::vector<vk::Framebuffer> m_framebuffers;
  vk::Rect2D m_targetRect;
  vk::RenderPass m_renderPass;
  vk::DescriptorPool m_descriptorPool;
  vk::ClearValue m_clearValue;
};

} // namespace Cory
