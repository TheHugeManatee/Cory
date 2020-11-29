#include "Descriptor.h"

#include "Image.h"
#include <ranges>

namespace Cory {

void DescriptorSet::setDescriptors(GraphicsContext &ctx,
                                   std::vector<UniformBufferBase *> uniformBuffers,
                                   std::vector<Texture *> textures)
{
    assert(uniformBuffers.size() == m_numUBOs &&
           "Supplied number of uniform buffers does not match the number supplied at "
           "initialization!");
    assert(textures.size() == m_numSamplers &&
           "Supplied number of texures does not match the number supplied at initialization!");

    std::vector<vk::DescriptorBufferInfo> bufferInfos;
    std::ranges::transform(
        uniformBuffers, std::back_inserter(bufferInfos),
        [](auto *c) -> auto { return vk::DescriptorBufferInfo(c->buffer(), 0, c->size()); });

    std::vector<vk::DescriptorImageInfo> imageInfos;
    std::ranges::transform(
        textures, std::back_inserter(imageInfos), [](auto *t) -> auto {
            return vk::DescriptorImageInfo(t->sampler(), t->view(),
                                           vk::ImageLayout::eShaderReadOnlyOptimal);
        });

    for (uint32_t i = 0; i < m_swapChainSize; ++i) {
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

} // namespace Cory