#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

#include <array>
#include <optional>
#include <vector>

inline VkResult CreateDebugUtilsMessengerEXT(VkInstance instance,
                                             const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
                                             const VkAllocationCallbacks *pAllocator,
                                             VkDebugUtilsMessengerEXT *pDebugMessenger)
{
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

inline void DestroyDebugUtilsMessengerEXT(VkInstance instance,
                                          VkDebugUtilsMessengerEXT debugMessenger,
                                          const VkAllocationCallbacks *pAllocator)
{
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> computeFamily;
    std::optional<uint32_t> transferFamily;
    std::optional<uint32_t> presentFamily;
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;

    static VkVertexInputBindingDescription getBindingDescription()
    {
        VkVertexInputBindingDescription bindingDescription{};

        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate =
            VK_VERTEX_INPUT_RATE_VERTEX; // alternative: INPUT_RATE_INSTANCE

        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions()
    {
        std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};

        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        // glsl <> attribute format mapping:
        // float: VK_FORMAT_R32_SFLOAT
        // vec2: VK_FORMAT_R32G32_SFLOAT
        // vec3: VK_FORMAT_R32G32B32_SFLOAT
        // vec4: VK_FORMAT_R32G32B32A32_SFLOAT
        // ivec2: VK_FORMAT_R32G32_SINT
        // uvec4: VK_FORMAT_R32G32B32A32_UINT
        // double: VK_FORMAT_R64_SFLOAT

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        return attributeDescriptions;
    }
};

struct Context {
    VkInstance instance;
    VkDevice device;
    VkPhysicalDevice physicalDevice{VK_NULL_HANDLE};
};

uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter,
                        VkMemoryPropertyFlags properties);

class Buffer {
  public:
    Buffer(Context& device, VkDeviceSize size, VkBufferUsageFlags usage,
           VkMemoryPropertyFlags properties);
    ~Buffer();

    // don't copy this thing
    Buffer(const Buffer &rhs) = delete;
    void operator=(const Buffer &rhs) = delete;
    
    // we could move technically, but I'm lazy
    Buffer(Buffer &&rhs) = delete;
    void operator=(Buffer &&rhs) = delete;

    void upload(void *data, uint32_t size, uint32_t offset);
    void download();

  private:
    VkBuffer m_buffer;
    VkDeviceMemory m_bufferMemory;
    VkDeviceSize m_size;
    VkBufferUsageFlags m_usage;         // maybe not needed
    VkMemoryPropertyFlags m_properties; // maybe not needed
};

namespace primitives {
inline std::vector<Vertex> triangle()
{
    return std::vector<Vertex>{{{0.0f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
                               {{0.5f, 0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
                               {{-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}}};
}
} // namespace primitives