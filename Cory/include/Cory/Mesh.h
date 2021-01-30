#pragma once

#include "Buffer.h"
#include "Context.h"

#include <vulkan/vulkan.hpp>

#include <array>
#include <functional>
#include <glm.h>

namespace Cory {

struct Vertex {
  glm::vec3 pos;
  glm::vec3 color;
  glm::vec2 texCoord;

  static vk::VertexInputBindingDescription GetBindingDescription();
  static std::vector<vk::VertexInputAttributeDescription>
  GetAttributeDescriptions();

  bool operator==(const Vertex &rhs) const;

  struct hasher {
    size_t operator()(Vertex const &vertex) const;
  };
};

class Mesh {
public:
  template <typename VertexType, typename IndexType = std::uint32_t>
  Mesh(GraphicsContext &ctx, const std::vector<VertexType> &vertices,
       const std::vector<IndexType> &indices, vk::PrimitiveTopology topology)
      : m_ctx{ctx}
      , m_topology{topology}
  {
    createVertexBuffer(vertices.data(), sizeof(VertexType) * vertices.size());
    createIndexBuffer(indices.data(), sizeof(IndexType) * indices.size());

    if constexpr (std::is_same_v<IndexType, std::uint8_t>)
      m_indexType = vk::IndexType::eUint8EXT; // looks like we would need an
                                              // extension for that though
    if constexpr (std::is_same_v<IndexType, std::uint16_t>)
      m_indexType = vk::IndexType::eUint16;
    if constexpr (std::is_same_v<IndexType, std::uint32_t>)
      m_indexType = vk::IndexType::eUint32;

    m_attributeDescriptions = typename VertexType::GetAttributeDescriptions();
    m_bindingDescription = typename VertexType::GetBindingDescription();
    m_numVertices = static_cast<uint32_t>(indices.size());
  }

  ~Mesh()
  {
    m_vertexBuffer.destroy(m_ctx);
    m_indexBuffer.destroy(m_ctx);
  }

  const auto &vertexBuffer() const { return m_vertexBuffer; }
  const auto &indexBuffer() const { return m_indexBuffer; }
  auto topology() const { return m_topology; }
  auto indexType() const { return m_indexType; }
  auto numVertices() const { return m_numVertices; }
  const auto &bindingDescription() const { return m_bindingDescription; }
  const auto &attributeDescriptions() const { return m_attributeDescriptions; }

private:
  void createVertexBuffer(const void *vertexData,
                          const vk::DeviceSize dataSize);

  void createIndexBuffer(const void *indexData, vk::DeviceSize dataSize);

private:
  GraphicsContext &m_ctx;

  uint32_t m_numVertices;
  Buffer m_vertexBuffer;
  Buffer m_indexBuffer;

  vk::VertexInputBindingDescription m_bindingDescription;
  std::vector<vk::VertexInputAttributeDescription> m_attributeDescriptions;
  vk::IndexType m_indexType;
  vk::PrimitiveTopology m_topology;
};

struct meshdata {
  std::vector<Vertex> vertices;
  std::vector<uint16_t> indices;
};

namespace primitives {

std::vector<Vertex> triangle();

meshdata quad();
meshdata doublequad();

} // namespace primitives

} // namespace Cory