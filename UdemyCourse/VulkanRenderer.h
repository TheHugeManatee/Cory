#pragma once

#include "cvk/instance.h"
#include <Cory/vk/device.h>

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
    GLFWwindow *window_;
    cory::vk::instance instance_;
    cory::vk::physical_device_info physicalDevice_;
    cory::vk::device device_;

    cory::vk::physical_device_info pickPhysicalDevice();
    cory::vk::device createLogicalDevice();
};
