#pragma once

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <Cory/Renderer/Common.hpp>

namespace Cory {
class DynamicGeometry {
  public:

#pragma pack(push, 1)
    struct Vertex {
        glm::vec3 pos;
        glm::vec3 normal;
        glm::vec4 col;
    };
#pragma pack(pop)
    // just verifying that the layout is ok - one vertex is supposed to be 10 floats
    static_assert(sizeof(Vertex) == 10 * sizeof(float));

    /// create an equilateral triangle mesh in XY
    static Magnum::Vk::Mesh createTriangle(Context &ctx, uint32_t binding = 0);
    /// create a unit cube centered around the @a offset
    static Magnum::Vk::Mesh createCube(Context &ctx, glm::vec3 offset = {}, uint32_t binding = 0);
};
} // namespace Cory