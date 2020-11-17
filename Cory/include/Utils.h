#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <stb_image.h>

#include <string>
#include <memory>

namespace Cory {

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

} // namespace Cory