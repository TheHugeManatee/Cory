#pragma once

#include <memory>
#include <vulkan/vulkan.hpp>

struct GLFWwindow;

namespace Cory {
class GraphicsContext;

class ImGuiLayer {
  public:
    ImGuiLayer();

    void Init(GLFWwindow *window, GraphicsContext &ctx, vk::RenderPass renderPass,
              uint32_t subpassIdx, vk::SampleCountFlagBits msaaSamples, uint32_t minImageCount);
    void Deinit(GraphicsContext &ctx);

    void NewFrame(GraphicsContext &ctx);
    void DrawFrame(GraphicsContext &ctx, vk::Framebuffer framebuffer, vk::Extent2D surfaceExtent);

  private:
    vk::RenderPass m_renderPass;
    vk::DescriptorPool m_descriptorPool;
    vk::ClearValue m_clearValue;
};

} // namespace Cory
