#pragma once

#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.h>
#include <optional>
#include <cstdint>

struct graphics_context {
    vk::DispatchLoaderDynamic dl; // the vulkan dynamic dispatch loader
    vk::UniqueInstance instance{};
    vk::PhysicalDevice physicalDevice{};
    VmaAllocator allocator{};

    vk::UniqueDevice device{};
    vk::UniqueCommandPool transientCmdPool{};

    vk::Queue graphicsQueue{};
    vk::Queue presentQueue{};
};

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> computeFamily;
    std::optional<uint32_t> transferFamily;
    std::optional<uint32_t> presentFamily;
};

struct SwapChainSupportDetails {
    vk::SurfaceCapabilitiesKHR capabilities;
    std::vector<vk::SurfaceFormatKHR> formats;
    std::vector<vk::PresentModeKHR> presentModes;
};