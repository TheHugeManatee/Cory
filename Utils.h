#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <glm/glm.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

// single-header includes
#include <stb_image.h>
#include <tiny_obj_loader.h>

struct Vertex;
class host_buffer;
class device_buffer;
class device_image;
struct stbi_image;

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
    glm::vec2 texCoord;

    static VkVertexInputBindingDescription getBindingDescription()
    {
        VkVertexInputBindingDescription bindingDescription{};

        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate =
            VK_VERTEX_INPUT_RATE_VERTEX; // alternative: INPUT_RATE_INSTANCE

        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions()
    {
        std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

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

        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

        return attributeDescriptions;
    }

    bool operator==(const Vertex &rhs) const
    {
        return rhs.pos == pos && rhs.color == color && rhs.texCoord == texCoord;
    }

    struct hasher {
        size_t operator()(Vertex const &vertex) const
        {
            return ((std::hash<glm::vec3>()(vertex.pos) ^
                     (std::hash<glm::vec3>()(vertex.color) << 1)) >>
                    1) ^
                   (std::hash<glm::vec2>()(vertex.texCoord) << 1);
        }
    };
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
VkFormat findSupportedFormat(VkPhysicalDevice physicalDevice,
                             const std::vector<VkFormat> &candidates, VkImageTiling tiling,
                             VkFormatFeatureFlags features);
VkFormat findDepthFormat(VkPhysicalDevice physicalDevice);
inline bool hasStencilComponent(VkFormat format)
{
    return format == VK_FORMAT_D16_UNORM_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT ||
           format == VK_FORMAT_S8_UINT;
}

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
    device_buffer &operator=(const device_buffer &rhs) = delete;

    // we could move technically
    device_buffer(device_buffer &&rhs) = default;
    device_buffer &operator=(device_buffer &&rhs) = default;

    void create(graphics_context &ctx, VkDeviceSize size, VkBufferUsageFlags usage,
                VkMemoryPropertyFlags properties);
    void destroy(graphics_context &ctx);

    void upload(graphics_context &ctx, const void *srcData, VkDeviceSize size,
                VkDeviceSize offset = 0);
    void download(graphics_context &ctx, host_buffer &buf);

    void copy_to(graphics_context &ctx, device_buffer &rhs, VkDeviceSize size);
    void copy_to(graphics_context &ctx, const device_image &rhs);

    VkBuffer buffer() { return m_buffer; };
    VkDeviceMemory memory() { return m_bufferMemory; }

  private:
    VkBuffer m_buffer{};
    VkDeviceMemory m_bufferMemory{};
    VkDeviceSize m_size{};
    VkBufferUsageFlags m_usage{};         // maybe not needed
    VkMemoryPropertyFlags m_properties{}; // maybe not needed
};

class device_image {
  public:
    device_image();
    ~device_image();

    // don't copy this thing
    device_image(const device_image &rhs) = delete;
    device_image &operator=(const device_image &rhs) = delete;

    // we could move technically
    device_image(device_image &&rhs) = default;
    device_image &operator=(device_image &&rhs) = default;

    void destroy(graphics_context &ctx);

    void transitionLayout(graphics_context &ctx, VkImageLayout newLayout);

    VkImage image() const { return m_image; };
    VkDeviceMemory memory() const { return m_imageMemory; }
    VkImageView view() const { return m_imageView; }
    VkSampler sampler() const { return m_sampler; }
    glm::uvec3 size() const { return m_size; }

  protected:
    VkImage m_image{};
    VkDeviceMemory m_imageMemory{};
    glm::uvec3 m_size{};
    uint32_t m_mipLevels{};
    VkFormat m_format{};
    VkImageLayout m_currentLayout{};
    VkImageView m_imageView{};
    VkSampler m_sampler{};
};

class device_texture : public device_image {
  public:
    void create(graphics_context &ctx, glm::uvec3 size, VkImageType type, VkFormat format,
                VkImageTiling tiling, VkFilter filter, VkSamplerAddressMode addressMode,
                VkImageUsageFlags usage, VkMemoryPropertyFlags properties);

    void upload(graphics_context &ctx, const void *srcData, VkDeviceSize size,
                VkDeviceSize offset = 0);
    // void download(graphics_context &ctx, host_buffer &buf);
    // void copy_to(graphics_context &ctx, device_buffer &rhs, VkDeviceSize size);

  private:
};

class device_depth : public device_image {
  public:
    void create(graphics_context &ctx, glm::uvec3 size, VkFormat format);
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
    std::vector<Vertex> vertices{{{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
                                 {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
                                 {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
                                 {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}};
    std::vector<uint16_t> indices{0, 1, 2, 2, 3, 0};
    return {vertices, indices};
}

inline mesh doublequad()
{
    std::vector<Vertex> vertices{{{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
                                 {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
                                 {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
                                 {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},

                                 {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
                                 {{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
                                 {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
                                 {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}};
    std::vector<uint16_t> indices{0, 1, 2, 2, 3, 0,

                                  4, 5, 6, 6, 7, 4};
    return {vertices, indices};
}

} // namespace primitives

struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

struct stbi_image {
  public:
    explicit stbi_image(const std::string &file);
    ~stbi_image();
    size_t size() { return size_t(width) * size_t(height) * 4; }

    int width{};
    int height{};
    int channels{};
    unsigned char *data{};
};

class SingleTimeCommandBuffer {
  public:
    SingleTimeCommandBuffer(graphics_context &ctx);
    ~SingleTimeCommandBuffer();

    VkCommandBuffer &buffer() { return m_commandBuffer; }

  private:
    graphics_context m_ctx;
    VkCommandBuffer m_commandBuffer;
};