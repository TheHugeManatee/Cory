#pragma once

#include "Buffer.h"

namespace Cory {
class GraphicsContext;
class Texture;

class UniformBufferBase {
public:
  void destroy(GraphicsContext &ctx) { m_buffer.destroy(ctx); };

  vk::Buffer buffer() const { return m_buffer.buffer(); }
  vk::DeviceSize size() const { return m_buffer.size(); }

protected:
  Buffer m_buffer;
};

template <typename DerivedT> class UniformBuffer : public UniformBufferBase {
  //     struct OffsetOfTester {
  //         int firstMember;
  //     };
  //    static constexpr FIRST_MEMBER_OFFSET = offsetof(OffsetOfTester,
  //    firstMember);

public:
  using DerivedType = DerivedT;

  UniformBuffer() {}
  //     {
  //         // TODO figure out whether there is a static_assert we can use to
  //         check that memory layout
  //         // is ok..
  //     }

  UniformBuffer(UniformBuffer<DerivedType> &&rhs) = default;
  UniformBuffer &operator=(UniformBuffer<DerivedType> &&) = default;

  void update(GraphicsContext &ctx)
  {
    // note: this is probably UB but it would allow a CRTP design..
    // m_buffer.upload(m_ctx, (std::byte*)&this + FIRST_MEMBER_OFFSET,
    // sizeof(DerivedType));

    m_buffer.upload(ctx, &m_data, sizeof(DerivedType));
  }

  void create(GraphicsContext &ctx)
  {
    m_buffer.create(ctx, sizeof(DerivedType),
                    vk::BufferUsageFlagBits::eUniformBuffer,
                    DeviceMemoryUsage::eCpuToGpu);
  };

  DerivedType &data() { return m_data; }

private:
  DerivedType m_data;
};

class DescriptorSet {
public:
  DescriptorSet() {}

  void create(GraphicsContext &ctx, uint32_t swapChainSize, uint32_t numUBOs,
              uint32_t numSamplers);

  void destroy(GraphicsContext &ctx);

  void setDescriptors(
      GraphicsContext &ctx,
      const std::vector<std::vector<const UniformBufferBase *>> &uniformBuffers,
      const std::vector<std::vector<const Texture *>> &textures);

  uint32_t numUBOs() { return m_numUBOs; }
  uint32_t numSamplers() { return m_numSamplers; }
  vk::DescriptorSet descriptorSet(size_t i) { return m_descriptorSets[i]; }
  vk::DescriptorSetLayout &layout() { return *m_descriptorSetLayout; }

private:
  void createPool(GraphicsContext &ctx);

  void createLayout(GraphicsContext &ctx);

  void allocateDescriptorSets(GraphicsContext &ctx);

private:
  uint32_t m_numUBOs;
  uint32_t m_numSamplers;
  uint32_t m_swapChainSize;

  vk::UniqueDescriptorPool m_descriptorPool;
  vk::UniqueDescriptorSetLayout m_descriptorSetLayout;
  std::vector<vk::DescriptorSet> m_descriptorSets;
};

} // namespace Cory