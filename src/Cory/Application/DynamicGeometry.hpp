#pragma once

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <Cory/Renderer/Common.hpp>
#include <KDGpu/buffer.h>
#include <KDGpu/graphics_pipeline_options.h>

namespace Cory {

// simple mesh structure with vertex and index buffers
struct Mesh {
#pragma pack(push, 1)
    struct Vertex {
        glm::vec3 pos;
        glm::vec3 normal;
        glm::vec4 col;
    };
#pragma pack(pop)
    // just verifying that the layout is ok - one vertex is supposed to be 10 floats
    static_assert(sizeof(Vertex) == 10 * sizeof(float));

    static constexpr std::vector<Gpu::VertexAttribute> vertexAttributes()
    {
        return {{
            {
                // position
                .location = 0,
                .format = Gpu::Format::R32G32B32_SFLOAT,
                .offset = offsetof(Mesh::Vertex, pos),
            },
            {
                // normal
                .location = 1,
                .format = Gpu::Format::R32G32B32_SFLOAT,
                .offset = offsetof(Mesh::Vertex, normal),
            },
            {
                // color
                .location = 2,
                .format = Gpu::Format::R32G32B32A32_SFLOAT,
                .offset = offsetof(Mesh::Vertex, col),
            },
        }};
    }

    Gpu::Buffer vertexBuffer;
    Gpu::Buffer indexBuffer;
    uint32_t vertexCount;
    uint32_t indexCount;
};

class DynamicGeometry {
  public:
    /// create an equilateral triangle mesh in XY
    static Mesh createTriangle(Context &ctx, uint32_t binding = 0);
    /// create a unit cube centered around the @a offset
    static Mesh createCube(Context &ctx, glm::vec3 offset = {}, uint32_t binding = 0);

    // Generic function that handles vertex and index buffer creation & upload
    static Mesh createFromCpuBuffers(Context &ctx,
                                     std::span<const Mesh::Vertex> vertexData,
                                     std::span<const uint32_t> indexData);
};
} // namespace Cory