#pragma once

#include <Cory/vk/instance.h>

// only pull in the typedefs
#include <vulkan/vulkan_core.h>
class GLFWwindow;


#include <GLFW/glfw3.h>
class VulkanRenderer {
  public:
    VulkanRenderer(GLFWwindow *window);

    ~VulkanRenderer();

  private:
    static cory::vk::instance createInstance();

  private:
    cory::vk::instance instance_;
    GLFWwindow *window_;
};
