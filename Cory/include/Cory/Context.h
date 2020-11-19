#pragma once

#include <cstdint>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

class GLFWwindow;

namespace Cory {

struct GraphicsContext {
    vk::DispatchLoaderDynamic dl; // the vulkan dynamic dispatch loader
    vk::UniqueInstance instance{};
    vk::PhysicalDevice physicalDevice{};
    VmaAllocator allocator{};

    vk::UniqueDevice device{};
    vk::UniqueCommandPool transientCmdPool{};
    vk::UniqueCommandPool permanentCmdPool{};

    vk::Queue graphicsQueue{};
    vk::Queue presentQueue{};
};

class SwapChain {
  public:
    SwapChain(GraphicsContext &ctx, GLFWwindow *window, vk::SurfaceKHR surface);
    ~SwapChain();

    auto swapchain() { return m_swapChain; }
    auto images() { return m_swapChainImages; }
    auto format() { return m_swapChainImageFormat; }
    auto extent() { return m_swapChainExtent; }
    auto views() { return m_swapChainImageViews; }
    auto size() { return m_swapChainImages.size(); }

  private:
    void createSwapchain(vk::SurfaceKHR surface);
    void createImageViews();

    vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR &capabilities);

    vk::PresentModeKHR
    chooseSwapPresentMode(const std::vector<vk::PresentModeKHR> &availablePresentModes);

    vk::SurfaceFormatKHR
    chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR> &availableFormats);

  private:
    GraphicsContext &m_ctx;

    GLFWwindow *m_window;
    vk::SwapchainKHR m_swapChain{};
    std::vector<vk::Image> m_swapChainImages{};
    vk::Format m_swapChainImageFormat{};
    vk::Extent2D m_swapChainExtent{};
    std::vector<vk::ImageView> m_swapChainImageViews{};
};

} // namespace Cory