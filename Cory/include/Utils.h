#pragma once

#include <vulkan/vulkan.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>

#include <vk_mem_alloc.h>

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

#include <fmt/format.h>
inline std::string formatBytes(size_t bytes)
{
    const static std::array<std::string_view, 5> suffix{"b", "KiB", "MiB", "GiB", "TiB"};
    uint32_t suff{};
    size_t remaind{};
    for (; suff < suffix.size() && bytes >= 1024; ++suff) {
        remaind = bytes % 1024;
        bytes /= 1024;
    }

    if (remaind == 0)
        return fmt::format("{} {}", bytes , suffix[suff]);

    return fmt::format("{:.2f} {}", float(bytes) + float(remaind) / 1024.f, suffix[suff]);
}

struct graphics_context {
    vk::DispatchLoaderDynamic dl; // the vulkan dynamic dispatch loader
    vk::UniqueInstance instance{};
    vk::PhysicalDevice physicalDevice{};
    VmaAllocator allocator {};
    
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

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;

    static vk::VertexInputBindingDescription getBindingDescription()
    {
        vk::VertexInputBindingDescription bindingDescription{};

        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate =
            vk::VertexInputRate::eVertex; // alternative: INPUT_RATE_INSTANCE

        return bindingDescription;
    }

    static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions()
    {
        std::array<vk::VertexInputAttributeDescription, 3> attributeDescriptions{};

        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = vk::Format::eR32G32B32Sfloat;
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
        attributeDescriptions[1].format = vk::Format::eR32G32B32Sfloat;
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = vk::Format::eR32G32Sfloat;
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

uint32_t findMemoryType(vk::PhysicalDevice physicalDevice, uint32_t typeFilter,
                        vk::MemoryPropertyFlags properties);
vk::Format findSupportedFormat(vk::PhysicalDevice physicalDevice,
                               const std::vector<vk::Format> &candidates, vk::ImageTiling tiling,
                               vk::FormatFeatureFlags features);
vk::Format findDepthFormat(vk::PhysicalDevice physicalDevice);
inline bool hasStencilComponent(vk::Format format)
{
    return format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD16UnormS8Uint ||
           format == vk::Format::eD24UnormS8Uint || format == vk::Format::eS8Uint;
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

enum class DeviceMemoryUsage : std::underlying_type<VmaMemoryUsage>::type {
    eUnknown = VMA_MEMORY_USAGE_UNKNOWN,                ///< should not be used
    eGpuOnly = VMA_MEMORY_USAGE_GPU_ONLY,               ///< textures, images used as attachments
    eCpuOnly = VMA_MEMORY_USAGE_CPU_ONLY,               ///< staging buffers
    eCpuToGpu = VMA_MEMORY_USAGE_CPU_TO_GPU,            ///< dynamic resources, i.e. vertex/uniform buffers, dynamic textures
    eGpuToCpu = VMA_MEMORY_USAGE_GPU_TO_CPU,            ///< transform feedback, screenshots etc.
    eCpuCopy = VMA_MEMORY_USAGE_CPU_COPY,               ///< staging custom paging/residency
    eGpuLazilyAllocated = VMA_MEMORY_USAGE_GPU_LAZILY_ALLOCATED ///< transient attachment images, might not be available for desktop GPUs
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

    void create(graphics_context &ctx, vk::DeviceSize size, vk::BufferUsageFlags usage,
                DeviceMemoryUsage memUsage);

    void destroy(graphics_context &ctx);

    void upload(graphics_context &ctx, const void *srcData, vk::DeviceSize size,
                vk::DeviceSize offset = 0);
    void download(graphics_context &ctx, host_buffer &buf);

    void copy_to(graphics_context &ctx, device_buffer &rhs, vk::DeviceSize size);
    void copy_to(graphics_context &ctx, const device_image &rhs);

    vk::Buffer buffer() { return m_buffer; };

  private:
    vk::Buffer m_buffer{};
    vk::DeviceSize m_size{};
    VmaAllocation m_allocation {};
    void* m_mappedMemory { };
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

    void transitionLayout(graphics_context &ctx, vk::ImageLayout newLayout);

    const vk::Image image() const { return m_image; };
    vk::DeviceMemory memory() const { return m_deviceMemory; }
    vk::ImageView view() const { return m_imageView; }
    vk::Sampler sampler() const { return m_sampler; }
    glm::uvec3 size() const { return m_size; }

  protected:
    vk::Image m_image{};
    vk::DeviceMemory m_deviceMemory{};
    VmaAllocation m_allocation{};
    glm::uvec3 m_size{};
    uint32_t m_mipLevels{};
    vk::Format m_format{};
    vk::ImageLayout m_currentLayout{};
    vk::ImageView m_imageView{};
    vk::Sampler m_sampler{};
    vk::SampleCountFlagBits m_samples{vk::SampleCountFlagBits::e1};

    std::string m_name;
};

class device_texture : public device_image {
  public:
    void create(graphics_context &ctx, glm::uvec3 size, uint32_t mipLevels, vk::ImageType type,
                vk::Format format, vk::ImageTiling tiling, vk::Filter filter,
                vk::SamplerAddressMode addressMode, vk::ImageUsageFlags usage,
                DeviceMemoryUsage memoryUsage);

    void upload(graphics_context &ctx, const void *srcData, vk::DeviceSize size,
                vk::DeviceSize offset = 0);
    // void download(graphics_context &ctx, host_buffer &buf);
    // void copy_to(graphics_context &ctx, device_buffer &rhs, vk::DeviceSize size);

    /**
     * generates mipmaps for a texture. dstLayout and dstAccess specify the configuration that the
     * texture should be transitioned to after the mipmap generation
     */
    void generate_mipmaps(graphics_context &ctx,
                          vk::ImageLayout dstLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                          vk::AccessFlags dstAccess = vk::AccessFlagBits::eShaderRead);

  private:
};

class render_target : public device_image {
  public:
    void create(graphics_context &ctx, glm::uvec3 size, vk::Format format,
                vk::SampleCountFlagBits msaaSamples);
};

class depth_buffer : public device_image {
  public:
    void create(graphics_context &ctx, glm::uvec3 size, vk::Format format,
                vk::SampleCountFlagBits msaaSamples);
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

    vk::CommandBuffer &buffer() { return *m_commandBuffer; }

    vk::CommandBuffer *operator->() { return &*m_commandBuffer; };

  private:
    graphics_context &m_ctx;
    vk::UniqueCommandBuffer m_commandBuffer;
};