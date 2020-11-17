#include "Mesh.h"

#include <glm.h>

namespace Cory {

size_t Vertex::hasher::operator()(Vertex const &vertex) const
{
    return ((std::hash<glm::vec3>()(vertex.pos) ^ (std::hash<glm::vec3>()(vertex.color) << 1)) >>
            1) ^
           (std::hash<glm::vec2>()(vertex.texCoord) << 1);
}

vk::VertexInputBindingDescription Vertex::getBindingDescription()
{
    vk::VertexInputBindingDescription bindingDescription{};

    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = vk::VertexInputRate::eVertex; // alternative: INPUT_RATE_INSTANCE

    return bindingDescription;
}

std::array<vk::VertexInputAttributeDescription, 3> Vertex::getAttributeDescriptions()
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

bool Vertex::operator==(const Vertex &rhs) const
{
    return rhs.pos == pos && rhs.color == color && rhs.texCoord == texCoord;
}

namespace primitives {

std::vector<Vertex> triangle()
{
    return std::vector<Vertex>{{{0.0f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
                               {{0.5f, 0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
                               {{-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}}};
}

mesh primitives::quad()
{
    std::vector<Vertex> vertices{{{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
                                 {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
                                 {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
                                 {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}};
    std::vector<uint16_t> indices{0, 1, 2, 2, 3, 0};
    return {vertices, indices};
}

mesh doublequad()
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
} // namespace Cory