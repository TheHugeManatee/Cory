#pragma once

#include <vulkan/vulkan.hpp>
#include <memory>

struct GLFWwindow;

namespace Cory {
class GraphicsContext;

class ImGuiLayer {
  public:
    ImGuiLayer();

    void Init(GLFWwindow *window, GraphicsContext &ctx, uint32_t queueFamily, vk::Queue queue,
              uint32_t minImageCount, vk::RenderPass renderPass);
    void Deinit(GraphicsContext &ctx);

    void NewFrame(GraphicsContext &ctx);
    void EndFrame(GraphicsContext &ctx);

  private:
    vk::DescriptorPool m_descriptorPool;
};

} // namespace Cory
