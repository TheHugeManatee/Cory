#pragma once

#include "Buffer.h"

namespace Cory {
class GraphicsContext;

template <typename DerivedType> class UniformBuffer {
//     struct OffsetOfTester {
//         int firstMember;
//     };
//    static constexpr FIRST_MEMBER_OFFSET = offsetof(OffsetOfTester, firstMember);

  public:
    
    UniformBuffer() { 
        // TODO figure out whether there is a static_assert we can use to check that memory layout is ok..
    }

    UniformBuffer(UniformBuffer<DerivedType>&& rhs) = default;
    UniformBuffer &operator=(UniformBuffer<DerivedType>&&) = default;

    void update(GraphicsContext &ctx)
    {
        // note: this is probably UB but it would allow a CRTP design..
        // m_buffer.upload(m_ctx, (std::byte*)&this + FIRST_MEMBER_OFFSET, sizeof(DerivedType));

        m_buffer.upload(ctx, &m_data, sizeof(DerivedType));
    }

    void create(GraphicsContext &ctx)
    {
        m_buffer.create(ctx, sizeof(DerivedType), vk::BufferUsageFlagBits::eUniformBuffer,
                        DeviceMemoryUsage::eCpuToGpu);
    };

    void destroy(GraphicsContext &ctx) { m_buffer.destroy(ctx); };

    vk::Buffer buffer() const { return m_buffer.buffer(); }

    DerivedType &data() { return m_data;}

  private:
    Buffer m_buffer;
    DerivedType m_data;
};

class DescriptorSet {
};

} // namespace Cory