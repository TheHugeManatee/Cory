#pragma once

#include <glm/vec2.hpp>
#include <string>

#include <Cory/Core/Common.hpp>

struct GLFWwindow;

namespace Cory {

class Window : NoCopy, NoMove {
  public:
    Window(glm::i32vec2 dimensions, std::string windowName);
    ~Window();

    [[nodiscard]] bool shouldClose() const;

  private:
    std::string windowName_;
    GLFWwindow *window_{};
};

} // namespace Cory
