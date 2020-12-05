#include "Context.h"

#include "Log.h"
#include "VkUtils.h"

#include <GLFW/glfw3.h>

namespace Cory {

SwapChain::SwapChain(GraphicsContext &ctx, GLFWwindow *window, vk::SurfaceKHR surface)
    : m_ctx{ctx}
    , m_window{window}
{
    createSwapchain(surface);
    createImageViews();
}

SwapChain::~SwapChain()
{
    for (auto imageView : m_swapChainImageViews) {
        m_ctx.device->destroyImageView(imageView);
    }

    m_ctx.device->destroySwapchainKHR(m_swapChain);
}

void SwapChain::createSwapchain(vk::SurfaceKHR surface)
{
    const SwapChainSupportDetails swapChainSupport =
        querySwapChainSupport(m_ctx.physicalDevice, surface);

    const vk::SurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    const vk::PresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    m_swapChainExtent = chooseSwapExtent(swapChainSupport.capabilities);
    m_swapChainImageFormat = surfaceFormat.format;

    CO_CORE_DEBUG("SwapChain configuration:");
    CO_CORE_DEBUG("    Surface Format:    {}, {}", to_string(surfaceFormat.format),
                  surfaceFormat.colorSpace);
    CO_CORE_DEBUG("    Present Mode:      {}", to_string(presentMode));
    CO_CORE_DEBUG("    Extent:            {}x{}", m_swapChainExtent.width,
                  m_swapChainExtent.height);

    // we use one more image as a buffer to avoid stalls when waiting for the next image to
    // become available
    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 &&
        imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    vk::SwapchainCreateInfoKHR createInfo;

    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = m_swapChainExtent;
    createInfo.imageArrayLayers = 1; // this might be 2 if we are developing stereoscopic stuff
    createInfo.imageUsage =
        vk::ImageUsageFlagBits::eColorAttachment; // for off-screen rendering, it is possible to
                                                  // use VK_IMAGE_USAGE_TRANSFER_DST_BIT instead

    uint32_t queueFamilyIndices[] = {m_ctx.queueFamilyIndices.graphicsFamily.value(),
                                     m_ctx.queueFamilyIndices.presentFamily.value()};

    // if the swap and present queues are different, the swap chain images have to be shareable
    if (m_ctx.queueFamilyIndices.graphicsFamily != m_ctx.queueFamilyIndices.presentFamily) {
        createInfo.imageSharingMode = vk::SharingMode::eConcurrent;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else {
        createInfo.imageSharingMode =
            vk::SharingMode::eExclusive; // exclusive has better performance
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha =
        vk::CompositeAlphaFlagBitsKHR::eOpaque; // whether the alpha channel should be used to
                                                // composite on top of other windows

    createInfo.presentMode = presentMode;
    createInfo.clipped =
        VK_TRUE; // false would force pixels to be rendered even if they are occluded. might be
                 // important if the buffer is read back somehow (screen shots etc?)

    createInfo.oldSwapchain = nullptr; // old swap chain, required when resizing etc.

    m_swapChain = m_ctx.device->createSwapchainKHR(createInfo);
}

vk::Extent2D SwapChain::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR &capabilities)
{
    // for high DPI, the extent between pixel size and screen coordinates might not be the same.
    // in this case, we need to compute a proper viewport extent from the GLFW screen
    // coordinates
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    }

    int width, height;
    glfwGetFramebufferSize(m_window, &width, &height);

    vk::Extent2D actualExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

    actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width,
                                    capabilities.maxImageExtent.width);
    actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height,
                                     capabilities.maxImageExtent.height);
    return actualExtent;
}

vk::PresentModeKHR
SwapChain::chooseSwapPresentMode(const std::vector<vk::PresentModeKHR> &availablePresentModes)
{
    // preferably use Mailbox, otherwise fall back to the first one
    if (std::ranges::count(availablePresentModes, vk::PresentModeKHR::eMailbox))
        return vk::PresentModeKHR::eMailbox;

    return availablePresentModes[0];
}

vk::SurfaceFormatKHR
SwapChain::chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR> &availableFormats)
{
    //     // BRGA8 and SRGB are the preferred formats, otherwise fall back to any available one
    if (std::ranges::count_if(availableFormats, [](const auto &fmt) {
            return fmt.format == vk::Format::eB8G8R8A8Srgb &&
                   fmt.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
        })) {
        return {vk::Format::eB8G8R8A8Srgb, vk::ColorSpaceKHR::eSrgbNonlinear};
    }

    return availableFormats[0];
}

void SwapChain::createImageViews()
{
    m_swapChainImages = m_ctx.device->getSwapchainImagesKHR(m_swapChain);

    m_swapChainImageViews.resize(m_swapChainImages.size());

    for (size_t i = 0; i < m_swapChainImages.size(); ++i) {
        vk::ImageViewCreateInfo createInfo{};
        createInfo.image = m_swapChainImages[i];
        createInfo.viewType = vk::ImageViewType::e2D;
        createInfo.format = m_swapChainImageFormat;
        createInfo.components.r = vk::ComponentSwizzle::eIdentity;
        createInfo.components.g = vk::ComponentSwizzle::eIdentity;
        createInfo.components.b = vk::ComponentSwizzle::eIdentity;
        createInfo.components.a = vk::ComponentSwizzle::eIdentity;

        // for stereographic, we could create separate image views for the array layers here
        createInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        m_swapChainImageViews[i] = m_ctx.device->createImageView(createInfo);
    }
}

} // namespace Cory