#pragma once

#include <cstdint>
#include <glm.h>
#include <stb_image.h>

#include <cmath>
#include <numbers>
#include <memory>
#include <string>
#include <vector>

namespace Cory {

std::string formatBytes(size_t bytes);

std::vector<char> readFile(const std::string &filename);

inline glm::vec3 sphericalToCartesian(const glm::vec3 spherical)
{
    auto r = spherical.x;
    auto theta = spherical.y;
    auto phi = spherical.z;
    float x = r * sin(phi) * cos(theta);
    float y = r * sin(phi) * sin(theta);
    float z = r * cos(phi);
    return {x, y, z};
}

inline glm::vec3 cartesianToSpherical(const glm::vec3 cartesian)
{
    float r = glm::length(cartesian);
    if (cartesian.x == 0.0 && cartesian.y == 0.0)
        return {r, 0.0, 0.0};

    float theta = atan(cartesian.y / cartesian.x);
    float phi = atan(sqrt(cartesian.x * cartesian.x + cartesian.y * cartesian.y) / cartesian.z);
    constexpr auto pif = std::numbers::pi_v<float>;
    if (cartesian.x < 0.0 && cartesian.y >= 0.0 && theta == 0.0) {
        theta = pif;
    }
    else if (cartesian.x < 0.0 && cartesian.y < 0.0 && theta > 0.0) {
        theta -= pif;
    }
    else if (cartesian.x < 0.0 && cartesian.y > 0.0 && theta < 0.0) {
        theta += pif;
    }
    return {r, theta, phi};
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