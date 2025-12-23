#include <Cory/Application/DynamicGeometry.hpp>

#include <Cory/Renderer/Context.hpp>
#include <KDGpu/buffer_options.h>

#include <glm/trigonometric.hpp> // for glm::radians
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <gsl/narrow>

namespace Cory {

Mesh DynamicGeometry::createTriangle(Context &ctx, uint32_t binding)
{
    std::vector<Mesh::Vertex> vertices{
        // Equilateral triangle in XY plane
        {{0.0f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},   // Top (red)
        {{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}}, // Left (green)
        {{0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},  // Right (blue)
    };
    std::vector<uint32_t> indices{0, 1, 2};
    // No offset for triangle, but could add if needed
    return createFromCpuBuffers(ctx, vertices, indices);
}

Mesh DynamicGeometry::createCube(Context &ctx, glm::vec3 offset, uint32_t binding)
{
    std::vector<Mesh::Vertex> vertices{
        // left face (white)
        {{-.5f, -.5f, -.5f}, {-1.0f, 0.0f, 0.0f}, {.9f, .9f, .9f, 1.0f}},
        {{-.5f, -.5f, .5f}, {-1.0f, 0.0f, 0.0f}, {.9f, .9f, .9f, 1.0f}},
        {{-.5f, .5f, .5f}, {-1.0f, 0.0f, 0.0f}, {.9f, .9f, .9f, 1.0f}},
        {{-.5f, -.5f, -.5f}, {-1.0f, 0.0f, 0.0f}, {.9f, .9f, .9f, 1.0f}},
        {{-.5f, .5f, .5f}, {-1.0f, 0.0f, 0.0f}, {.9f, .9f, .9f, 1.0f}},
        {{-.5f, .5f, -.5f}, {-1.0f, 0.0f, 0.0f}, {.9f, .9f, .9f, 1.0f}},

        // right face (yellow)
        {{.5f, -.5f, -.5f}, {1.0f, 0.0f, 0.0f}, {.8f, .8f, .1f, 1.0f}},
        {{.5f, .5f, .5f}, {1.0f, 0.0f, 0.0f}, {.8f, .8f, .1f, 1.0f}},
        {{.5f, -.5f, .5f}, {1.0f, 0.0f, 0.0f}, {.8f, .8f, .1f, 1.0f}},
        {{.5f, -.5f, -.5f}, {1.0f, 0.0f, 0.0f}, {.8f, .8f, .1f, 1.0f}},
        {{.5f, .5f, -.5f}, {1.0f, 0.0f, 0.0f}, {.8f, .8f, .1f, 1.0f}},
        {{.5f, .5f, .5f}, {1.0f, 0.0f, 0.0f}, {.8f, .8f, .1f, 1.0f}},

        // top face (orange, remember y axis points down)
        {{-.5f, -.5f, -.5f}, {0.0f, -1.0f, 0.0f}, {.9f, .6f, .1f, 1.0f}},
        {{.5f, -.5f, .5f}, {0.0f, -1.0f, 0.0f}, {.9f, .6f, .1f, 1.0f}},
        {{-.5f, -.5f, .5f}, {0.0f, -1.0f, 0.0f}, {.9f, .6f, .1f, 1.0f}},
        {{-.5f, -.5f, -.5f}, {0.0f, -1.0f, 0.0f}, {.9f, .6f, .1f, 1.0f}},
        {{.5f, -.5f, -.5f}, {0.0f, -1.0f, 0.0f}, {.9f, .6f, .1f, 1.0f}},
        {{.5f, -.5f, .5f}, {0.0f, -1.0f, 0.0f}, {.9f, .6f, .1f, 1.0f}},

        // bottom face (red)
        {{-.5f, .5f, -.5f}, {0.0f, 1.0f, 0.0f}, {.8f, .1f, .1f, 1.0f}},
        {{-.5f, .5f, .5f}, {0.0f, 1.0f, 0.0f}, {.8f, .1f, .1f, 1.0f}},
        {{.5f, .5f, .5f}, {0.0f, 1.0f, 0.0f}, {.8f, .1f, .1f, 1.0f}},
        {{-.5f, .5f, -.5f}, {0.0f, 1.0f, 0.0f}, {.8f, .1f, .1f, 1.0f}},
        {{.5f, .5f, .5f}, {0.0f, 1.0f, 0.0f}, {.8f, .1f, .1f, 1.0f}},
        {{.5f, .5f, -.5f}, {0.0f, 1.0f, 0.0f}, {.8f, .1f, .1f, 1.0f}},

        // nose face (blue)
        {{-.5f, -.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {.1f, .1f, .8f, 1.0f}},
        {{.5f, .5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {.1f, .1f, .8f, 1.0f}},
        {{-.5f, .5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {.1f, .1f, .8f, 1.0f}},
        {{-.5f, -.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {.1f, .1f, .8f, 1.0f}},
        {{.5f, -.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {.1f, .1f, .8f, 1.0f}},
        {{.5f, .5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {.1f, .1f, .8f, 1.0f}},

        // tail face (green)
        {{-.5f, -.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {.1f, .8f, .1f, 1.0f}},
        {{-.5f, .5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {.1f, .8f, .1f, 1.0f}},
        {{.5f, .5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {.1f, .8f, .1f, 1.0f}},
        {{-.5f, -.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {.1f, .8f, .1f, 1.0f}},
        {{.5f, .5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {.1f, .8f, .1f, 1.0f}},
        {{.5f, -.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {.1f, .8f, .1f, 1.0f}},

    };
    // 36 vertices, 12 triangles (2 per face), each triangle needs 3 indices
    std::vector<uint32_t> indices;
    for (uint32_t i = 0; i < vertices.size(); i += 3) {
        indices.push_back(i);
        indices.push_back(i + 1);
        indices.push_back(i + 2);
    }
    // apply offset
    for (auto &v : vertices) {
        v.pos += offset;
    }

    return createFromCpuBuffers(ctx, vertices, indices);
}

Mesh DynamicGeometry::createFromCpuBuffers(Context &ctx,
                                           std::span<const Mesh::Vertex> vertexData,
                                           std::span<const uint32_t> indexData)
{
    auto &device = ctx.device();
    KDGpu::UploadStagingBuffer vertex_staging_buffer;
    KDGpu::UploadStagingBuffer index_staging_buffer;
    Mesh mesh{
        .vertexCount = gsl::narrow_cast<uint32_t>(vertexData.size()),
        .indexCount = gsl::narrow_cast<uint32_t>(indexData.size()),
    };

    {
        const KDGpu::DeviceSize dataByteSize = vertexData.size() * sizeof(Mesh::Vertex);
        const KDGpu::BufferOptions bufferOptions = {
            .label = "Vertex Buffer",
            .size = dataByteSize,
            .usage = KDGpu::BufferUsageFlagBits::VertexBufferBit |
                     KDGpu::BufferUsageFlagBits::TransferDstBit,
            .memoryUsage = KDGpu::MemoryUsage::GpuOnly};

        mesh.vertexBuffer = device.createBuffer(bufferOptions);

        const KDGpu::BufferUploadOptions uploadOptions = {
            .destinationBuffer = mesh.vertexBuffer,
            .dstStages = KDGpu::PipelineStageFlagBit::VertexAttributeInputBit,
            .dstMask = KDGpu::AccessFlagBit::VertexAttributeReadBit,
            .data = vertexData.data(),
            .byteSize = dataByteSize};

        vertex_staging_buffer = ctx.graphicsQueue().uploadBufferData(uploadOptions);
    }
    // Create a buffer to hold the geometry index data
    {
        const KDGpu::DeviceSize dataByteSize = indexData.size() * sizeof(uint32_t);
        const KDGpu::BufferOptions bufferOptions = {.label = "Index Buffer",
                                                    .size = dataByteSize,
                                                    .usage =
                                                        KDGpu::BufferUsageFlagBits::IndexBufferBit |
                                                        KDGpu::BufferUsageFlagBits::TransferDstBit,
                                                    .memoryUsage = KDGpu::MemoryUsage::GpuOnly};
        mesh.indexBuffer = device.createBuffer(bufferOptions);
        const KDGpu::BufferUploadOptions uploadOptions = {
            .destinationBuffer = mesh.indexBuffer,
            .dstStages = KDGpu::PipelineStageFlagBit::IndexInputBit,
            .dstMask = KDGpu::AccessFlagBit::IndexReadBit,
            .data = indexData.data(),
            .byteSize = dataByteSize};
        index_staging_buffer = ctx.graphicsQueue().uploadBufferData(uploadOptions);
    }

    // Ensure upload is finished.
    vertex_staging_buffer.fence.wait();
    index_staging_buffer.fence.wait();

    return mesh;
}

} // namespace Cory