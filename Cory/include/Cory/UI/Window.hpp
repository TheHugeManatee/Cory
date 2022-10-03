#pragma once

#include <Cory/Base/Common.hpp>
#include <Cory/Core/VulkanUtils.hpp>

#include <glm/vec2.hpp>
#include <memory>
#include <string>

struct GLFWwindow;
typedef struct VkSurfaceKHR_T *VkSurfaceKHR;

namespace Cory {

class Context;
class SwapChain;

class Window : NoCopy, NoMove {
  public:
    Window(Context &context, glm::i32vec2 dimensions, std::string windowName);
    ~Window();

    [[nodiscard]] bool shouldClose() const;

    glm::i32vec2 dimensions() { return dimensions_; }

  private:
    void createSurface();
    void createSwapChain();

    Context &ctx_;
    std::string windowName_;
    glm::i32vec2 dimensions_;
    GLFWwindow *window_{};
    BasicVkObjectWrapper<VkSurfaceKHR> surface_{};
    std::unique_ptr<SwapChain> swapChain_;
};

} // namespace Cory
