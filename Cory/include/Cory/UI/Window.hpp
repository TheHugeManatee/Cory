#pragma once

#include <string>
#include <glm/vec2.hpp>

struct GLFWwindow;

namespace Cory {

class Window {
  public:
    Window(glm::i32vec2 dimensions, std::string windowName);
    ~Window();

    bool shouldClose();

  private:
    std::string windowName_;
    GLFWwindow* window_{};
};

} // namespace Cory
