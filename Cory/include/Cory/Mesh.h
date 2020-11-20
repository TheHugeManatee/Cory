#pragma once

#include <vulkan/vulkan.hpp>

#include <array>
#include <functional>
#include <glm.h>

namespace Cory {

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;

    static vk::VertexInputBindingDescription getBindingDescription();

    static std::vector<vk::VertexInputAttributeDescription> getAttributeDescriptions();

    bool operator==(const Vertex &rhs) const;

    struct hasher {
        size_t operator()(Vertex const &vertex) const;
    };
};

struct mesh {
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
};

namespace primitives {

std::vector<Vertex> triangle();

mesh quad();

mesh doublequad();

} // namespace primitives

} // namespace Cory