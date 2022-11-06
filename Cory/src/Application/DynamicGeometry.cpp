#include <Cory/Application/DynamicGeometry.hpp>

#include <Cory/Renderer/Context.hpp>

#include <glm/trigonometric.hpp> // for glm::radians
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <Corrade/Containers/Array.h>
#include <Magnum/Vk/Buffer.h>
#include <Magnum/Vk/BufferCreateInfo.h>
#include <Magnum/Vk/Mesh.h>
#include <Magnum/Vk/VertexFormat.h>

namespace Cory {
namespace Vk = Magnum::Vk;

namespace {
Vk::MeshLayout defaultMeshLayout(uint32_t binding)
{
    return Vk::MeshLayout{Vk::MeshPrimitive::Triangles}
        .addBinding(binding, sizeof(DynamicGeometry::Vertex))
        .addAttribute(0, binding, Vk::VertexFormat::Vector3, 0)
        .addAttribute(1, binding, Vk::VertexFormat::Vector3, 3 * sizeof(float))
        .addAttribute(2, binding, Vk::VertexFormat::Vector4, 6 * sizeof(float));
}
} // namespace

Vk::Mesh DynamicGeometry::createTriangle(Context &ctx, uint32_t binding)
{
    Vk::Mesh mesh(defaultMeshLayout(binding));

    const uint64_t numVertices = 4;
    Vk::Buffer vBuffer{
        ctx.device(),
        Vk::BufferCreateInfo{Vk::BufferUsage::VertexBuffer, numVertices * sizeof(Vertex)},
        Magnum::Vk::MemoryFlag::HostCoherent | Magnum::Vk::MemoryFlag::HostVisible};

    Corrade::Containers::Array<char, Vk::MemoryMapDeleter> data = vBuffer.dedicatedMemory().map();

    auto &view = reinterpret_cast<std::array<Vertex, 3> &>(*data.data());
    glm::vec2 p0{0, 0.5f};
    glm::vec2 p1{
        p0.x * cos(glm::radians(120.0f)) - p0.y * sin(glm::radians(120.0f)),
        p0.x * sin(glm::radians(120.0f)) + p0.y * cos(glm::radians(120.0f)),
    };
    glm::vec2 p2{
        p0.x * cos(glm::radians(240.0f)) - p0.y * sin(glm::radians(240.0f)),
        p0.x * sin(glm::radians(240.0f)) + p0.y * cos(glm::radians(240.0f)),
    };
    view[0] = Vertex{{p0.x, p0.y, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}};
    view[1] = Vertex{{p1.x, p1.y, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}};
    view[2] = Vertex{{p2.x, p2.y, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}};
    mesh.addVertexBuffer(0, std::move(vBuffer), 0).setCount(numVertices);

    return mesh;
}

Vk::Mesh DynamicGeometry::createCube(Context &ctx, glm::vec3 offset, uint32_t binding)
{
    struct VertexTemp {
        glm::vec3 pos;
        glm::vec3 col;
    };
    std::vector<VertexTemp> vertices{
        // left face (white)
        {{-.5f, -.5f, -.5f}, {.9f, .9f, .9f}},
        {{-.5f, -.5f, .5f}, {.9f, .9f, .9f}},
        {{-.5f, .5f, .5f}, {.9f, .9f, .9f}},
        {{-.5f, -.5f, -.5f}, {.9f, .9f, .9f}},
        {{-.5f, .5f, .5f}, {.9f, .9f, .9f}},
        {{-.5f, .5f, -.5f}, {.9f, .9f, .9f}},

        // right face (yellow)
        {{.5f, -.5f, -.5f}, {.8f, .8f, .1f}},
        {{.5f, .5f, .5f}, {.8f, .8f, .1f}},
        {{.5f, -.5f, .5f}, {.8f, .8f, .1f}},
        {{.5f, -.5f, -.5f}, {.8f, .8f, .1f}},
        {{.5f, .5f, -.5f}, {.8f, .8f, .1f}},
        {{.5f, .5f, .5f}, {.8f, .8f, .1f}},

        // top face (orange, remember y axis points down)
        {{-.5f, -.5f, -.5f}, {.9f, .6f, .1f}},
        {{.5f, -.5f, .5f}, {.9f, .6f, .1f}},
        {{-.5f, -.5f, .5f}, {.9f, .6f, .1f}},
        {{-.5f, -.5f, -.5f}, {.9f, .6f, .1f}},
        {{.5f, -.5f, -.5f}, {.9f, .6f, .1f}},
        {{.5f, -.5f, .5f}, {.9f, .6f, .1f}},

        // bottom face (red)
        {{-.5f, .5f, -.5f}, {.8f, .1f, .1f}},
        {{-.5f, .5f, .5f}, {.8f, .1f, .1f}},
        {{.5f, .5f, .5f}, {.8f, .1f, .1f}},
        {{-.5f, .5f, -.5f}, {.8f, .1f, .1f}},
        {{.5f, .5f, .5f}, {.8f, .1f, .1f}},
        {{.5f, .5f, -.5f}, {.8f, .1f, .1f}},

        // nose face (blue)
        {{-.5f, -.5f, 0.5f}, {.1f, .1f, .8f}},
        {{.5f, .5f, 0.5f}, {.1f, .1f, .8f}},
        {{-.5f, .5f, 0.5f}, {.1f, .1f, .8f}},
        {{-.5f, -.5f, 0.5f}, {.1f, .1f, .8f}},
        {{.5f, -.5f, 0.5f}, {.1f, .1f, .8f}},
        {{.5f, .5f, 0.5f}, {.1f, .1f, .8f}},

        // tail face (green)
        {{-.5f, -.5f, -0.5f}, {.1f, .8f, .1f}},
        {{-.5f, .5f, -0.5f}, {.1f, .8f, .1f}},
        {{.5f, .5f, -0.5f}, {.1f, .8f, .1f}},
        {{-.5f, -.5f, -0.5f}, {.1f, .8f, .1f}},
        {{.5f, .5f, -0.5f}, {.1f, .8f, .1f}},
        {{.5f, -.5f, -0.5f}, {.1f, .8f, .1f}},

    };
    for (auto &v : vertices) {
        v.pos += offset;
    }

    Vk::Mesh mesh(defaultMeshLayout(binding));

    Vk::Buffer vBuffer{
        ctx.device(),
        Vk::BufferCreateInfo{Vk::BufferUsage::VertexBuffer, vertices.size() * sizeof(Vertex)},
        Magnum::Vk::MemoryFlag::HostCoherent | Magnum::Vk::MemoryFlag::HostVisible};

    Corrade::Containers::Array<char, Vk::MemoryMapDeleter> data = vBuffer.dedicatedMemory().map();

    auto view = reinterpret_cast<Vertex*>(data.data());
    for (gsl::index i = 0; i < vertices.size(); ++i) {
        view[i] = Vertex{.pos = vertices[i].pos + offset,
                         .tex = vertices[i].pos + glm::vec3{0.5f, 0.5f, 0.5f},
                         .col = glm::vec4{vertices[i].col, 1.0f}};
    }

    mesh.addVertexBuffer(0, std::move(vBuffer), 0).setCount(vertices.size());

    return mesh;
}

} // namespace Cory