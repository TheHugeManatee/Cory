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
    //    static constexpr FIRST_MEMBER_OFFSET = offsetof(OffsetOfTester, firstMember);

  public:
    using DerivedType = DerivedT;

    UniformBuffer()
    {
        // TODO figure out whether there is a static_assert we can use to check that memory layout
        // is ok..
    }

    UniformBuffer(UniformBuffer<DerivedType> &&rhs) = default;
    UniformBuffer &operator=(UniformBuffer<DerivedType> &&) = default;

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

    DerivedType &data() { return m_data; }

  private:
    DerivedType m_data;
};

class DescriptorSet {
  public:
    DescriptorSet(GraphicsContext &ctx) {}

    void create(GraphicsContext &ctx, uint32_t swapChainSize, uint32_t numUBOs,
                uint32_t numSamplers)
    {
        m_swapChainSize = swapChainSize;
        m_numUBOs = numUBOs;
        m_numSamplers = numSamplers;

        createPool(ctx);
        createLayout(ctx);
        allocateDescriptorSets(ctx);
    }

    void setDescriptors(GraphicsContext &ctx, std::vector<UniformBufferBase *> uniformBuffers,
                        std::vector<Texture *> textures);;

    uint32_t numUBOs() { return m_numUBOs; }
    uint32_t numSamplers() { return m_numSamplers; }

  private:
    void createPool(GraphicsContext &ctx)
    {
        std::array<vk::DescriptorPoolSize, 2> poolSizes;
        poolSizes[0].type = vk::DescriptorType::eUniformBuffer;
        poolSizes[0].descriptorCount = m_numUBOs * m_swapChainSize;
        poolSizes[1].type = vk::DescriptorType::eCombinedImageSampler;
        poolSizes[1].descriptorCount = m_numSamplers * m_swapChainSize;

        vk::DescriptorPoolCreateInfo poolInfo{};
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = m_swapChainSize;
        // enables creation and freeing of individual descriptor sets -- we don't care for that
        // right now poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

        m_descriptorPool = ctx.device->createDescriptorPoolUnique(poolInfo);
    }

    void createLayout(GraphicsContext &ctx)
    {
        vk::DescriptorSetLayoutBinding uboLayoutBinding{};
        uboLayoutBinding.binding = 0;
        uboLayoutBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
        uboLayoutBinding.descriptorCount = m_numUBOs;
        uboLayoutBinding.stageFlags =
            vk::ShaderStageFlagBits::eVertex |
            vk::ShaderStageFlagBits::eFragment;        // or VK_SHADER_STAGE_ALL_GRAPHICS
        uboLayoutBinding.pImmutableSamplers = nullptr; // idk, something with image sampling

        vk::DescriptorSetLayoutBinding samplerBinding{};
        samplerBinding.binding = 1;
        samplerBinding.descriptorCount = m_numSamplers;
        samplerBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        samplerBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;
        samplerBinding.pImmutableSamplers = nullptr;

        std::array<vk::DescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding, samplerBinding};

        vk::DescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        m_descriptorSetLayout = ctx.device->createDescriptorSetLayoutUnique(layoutInfo);
    };

    void allocateDescriptorSets(GraphicsContext &ctx)
    {
        std::vector<vk::DescriptorSetLayout> layouts(m_swapChainSize, *m_descriptorSetLayout);

        vk::DescriptorSetAllocateInfo allocInfo{};
        allocInfo.descriptorPool = *m_descriptorPool;
        allocInfo.descriptorSetCount = m_swapChainSize;
        allocInfo.pSetLayouts = layouts.data();

        // NOTE: descriptor sets are freed implicitly when the pool is freed.
        m_descriptorSets = ctx.device->allocateDescriptorSets(allocInfo);
    }

  private:
    uint32_t m_numUBOs;
    uint32_t m_numSamplers;
    uint32_t m_swapChainSize;

    vk::UniqueDescriptorPool m_descriptorPool;
    vk::UniqueDescriptorSetLayout m_descriptorSetLayout;
    std::vector<vk::DescriptorSet> m_descriptorSets;
};

} // namespace Cory