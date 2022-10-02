#pragma once

#include <glm/vec2.hpp>
#include <string>

#include <Cory/Core/Common.hpp>

struct GLFWwindow;
typedef struct VkSurfaceKHR_T* VkSurfaceKHR;

namespace Cory {

class Context;

class Window : NoCopy, NoMove {
  public:
    Window(Context &context, glm::i32vec2 dimensions, std::string windowName);
    ~Window();

    [[nodiscard]] bool shouldClose() const;

  private:
    void createSurface();

    Context& ctx_;
    std::string windowName_;
    GLFWwindow *window_{};
    VkSurfaceKHR surface_{};
};

} // namespace Cory
