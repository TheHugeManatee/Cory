#include "Descriptor.h"

#include "Image.h"
#include <ranges>

namespace Cory {

void DescriptorSet::create(GraphicsContext &ctx, uint32_t swapChainSize, uint32_t numUBOs,
                           uint32_t numSamplers)
{
    m_swapChainSize = swapChainSize;
    m_numUBOs = numUBOs;
    m_numSamplers = numSamplers;

    createPool(ctx);
    createLayout(ctx);
    allocateDescriptorSets(ctx);
}

void DescriptorSet::setDescriptors(GraphicsContext &ctx,
                                   const std::vector<std::vector<const UniformBufferBase *>> &uniformBuffers,
                                   const std::vector<std::vector<const Texture *>> &textures)
{
    assert(uniformBuffers.size() == m_swapChainSize && textures.size() == m_swapChainSize &&
           "Supplied uniform / texture sets does not match swap chain size");
    assert(uniformBuffers[0].size() == m_numUBOs &&
           "Supplied number of uniform buffers does not match the number supplied at "
           "initialization!");
    assert(textures[0].size() == m_numSamplers &&
           "Supplied number of textures does not match the number supplied at initialization!");

    for (uint32_t i = 0; i < m_swapChainSize; ++i) {
        std::vector<vk::DescriptorBufferInfo> bufferInfos;
        std::ranges::transform(
            uniformBuffers[i], std::back_inserter(bufferInfos),
            [](auto *c) -> auto { return vk::DescriptorBufferInfo(c->buffer(), 0, c->size()); });

        std::vector<vk::DescriptorImageInfo> imageInfos;
        std::ranges::transform(
            textures[i], std::back_inserter(imageInfos), [](auto *t) -> auto {
                return vk::DescriptorImageInfo(t->sampler(), t->view(),
                                               vk::ImageLayout::eShaderReadOnlyOptimal);
            });

        std::array<vk::WriteDescriptorSet, 2> descriptorWrites;
        descriptorWrites[0].dstSet = m_descriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = vk::DescriptorType::eUniformBuffer;
        descriptorWrites[0].descriptorCount = m_numUBOs;
        descriptorWrites[0].pBufferInfo = bufferInfos.data();
        descriptorWrites[0].pImageInfo = nullptr;
        descriptorWrites[0].pTexelBufferView = nullptr;
        descriptorWrites[0].pNext = nullptr;

        descriptorWrites[1].dstSet = m_descriptorSets[i];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = vk::DescriptorType::eCombinedImageSampler;
        descriptorWrites[1].descriptorCount = m_numSamplers;
        descriptorWrites[1].pBufferInfo = nullptr;
        descriptorWrites[1].pImageInfo = imageInfos.data();
        descriptorWrites[1].pTexelBufferView = nullptr;
        descriptorWrites[1].pNext = nullptr;

        ctx.device->updateDescriptorSets(
            descriptorWrites, {}); // todo: this could be refactored to only one update call
    }
}

void DescriptorSet::createPool(GraphicsContext &ctx)
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

void DescriptorSet::createLayout(GraphicsContext &ctx)
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
}

void DescriptorSet::allocateDescriptorSets(GraphicsContext &ctx)
{
    std::vector<vk::DescriptorSetLayout> layouts(m_swapChainSize, *m_descriptorSetLayout);

    vk::DescriptorSetAllocateInfo allocInfo{};
    allocInfo.descriptorPool = *m_descriptorPool;
    allocInfo.descriptorSetCount = m_swapChainSize;
    allocInfo.pSetLayouts = layouts.data();

    // NOTE: descriptor sets are freed implicitly when the pool is freed.
    m_descriptorSets = ctx.device->allocateDescriptorSets(allocInfo);
}

} // namespace Cory