#pragma once

#include <cvk/context.h>
#include <cvk/instance.h>

// only pull in the typedefs
#include <vulkan/vulkan_core.h>
class GLFWwindow;

#include <GLFW/glfw3.h>
class VulkanRenderer {
  public:
    VulkanRenderer(GLFWwindow *window);

    ~VulkanRenderer();

  private:
    static cvk::instance createInstance();
    cvk::context createContext();

  private:
    GLFWwindow *window_;
    cvk::instance instance_;
    cvk::surface surface_;
    cvk::context context_;
};
