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
std::string formatBytes(size_t bytes);




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
