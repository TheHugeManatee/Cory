#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

#include <array>
#include <memory>
#include <optional>
#include <cstdint>
#include <utility>
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
    return VK_ERROR_EXTENSION_NOT_PRESENT;
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

struct graphics_context {
    VkInstance instance{};
    VkDevice device{};
    VkPhysicalDevice physicalDevice{VK_NULL_HANDLE};
    VkCommandPool transientCmdPool{};

    VkQueue graphicsQueue;
    VkQueue presentQueue;
};

uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter,
                        VkMemoryPropertyFlags properties);

class host_buffer {
  public:
    host_buffer(size_t size)
        : m_size{size}
        , m_data{std::make_unique<uint8_t[]>(m_size)} {};
    ~host_buffer(){};

    uint8_t *data() { return m_data.get(); };
    size_t size() { return m_size; }

  private:
    size_t m_size;
    std::unique_ptr<uint8_t[]> m_data;
};

class device_buffer {
  public:
    device_buffer();
    ~device_buffer();

    // don't copy this thing
    device_buffer(const device_buffer &rhs) = delete;
    device_buffer& operator=(const device_buffer &rhs) = delete;

    // we could move technically
    device_buffer(device_buffer &&rhs) = default;
    device_buffer& operator=(device_buffer &&rhs) = default;

    void create(graphics_context &ctx, VkDeviceSize size, VkBufferUsageFlags usage,
                VkMemoryPropertyFlags properties);
    void destroy(graphics_context &ctx);

    void upload(graphics_context &ctx, const void *srcData, VkDeviceSize size,
                VkDeviceSize offset = 0);
    void download(graphics_context &ctx, host_buffer &buf);

    void copy_to(graphics_context &ctx, device_buffer &rhs, VkDeviceSize size);

    VkBuffer buffer() { return m_buffer; };
    VkDeviceMemory memory() { return m_bufferMemory; }

  private:
    VkBuffer m_buffer{};
    VkDeviceMemory m_bufferMemory{};
    VkDeviceSize m_size{};
    VkBufferUsageFlags m_usage{};         // maybe not needed
    VkMemoryPropertyFlags m_properties{}; // maybe not needed
};

namespace primitives {

struct mesh {
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
};

inline std::vector<Vertex> triangle()
{
    return std::vector<Vertex>{{{0.0f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
                               {{0.5f, 0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
                               {{-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}}};
}

inline mesh quad()
{
    std::vector<Vertex> vertices{{{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
                                 {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
                                 {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}},
                                 {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}}};
    std::vector<uint16_t> indices{0, 1, 2, 2, 3, 0};
    return {vertices, indices};
}
} // namespace primitives

struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};