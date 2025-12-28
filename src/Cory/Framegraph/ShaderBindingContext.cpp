#include "ShaderBindingContext.hpp"

#include <Cory/Renderer/DescriptorSets.hpp>

#include <KDGpu/buffer.h>
#include <KDGpu/buffer_options.h>
#include <KDGpu/device.h>
#include <KDGpu/vulkan/vulkan_graphics_api.h>

namespace Cory {

namespace {
Gpu::Buffer createPerDrawDataBuffer(Gpu::Device &device, size_t size)
{
    using enum Gpu::BufferUsageFlagBits;
    return device.createBuffer(KDGpu::BufferOptions{
        .label = "ShaderBindingContext Per-Draw Data Buffer",
        .size = size,
        .usage = ShaderDeviceAddressBit | StorageBufferBit,
        .memoryUsage = Gpu::MemoryUsage::CpuToGpu,
    });
}
} // namespace

ShaderBindingContext::ShaderBindingContext(Gpu::Device &device,
                                           DescriptorSets &descriptorSets,
                                           uint32_t instanceIndex,
                                           size_t drawDataBufferSize)
    : device_{&device}
    , perDrawDataBuffer_(createPerDrawDataBuffer(device, drawDataBufferSize))
    , allocator_(GpuAllocation{
          .cpu = static_cast<uint8_t *>(perDrawDataBuffer_.map()),
          .gpu = perDrawDataBuffer_.bufferDeviceAddress(),
          .size = drawDataBufferSize,
      })
    , descriptorSets_(&descriptorSets)
    , instanceIndex_(instanceIndex)
{
}

ShaderBindingContext::~ShaderBindingContext() {}

TextureHeapIndex ShaderBindingContext::bindTexture2D(Gpu::TextureViewHandle textureHandle)
{
    TextureHeapIndex textureIndex{nextTextureIndex_++};

    descriptorSets_->write(ImageBindPoint::Texture2D,
                           instanceIndex_,
                           textureIndex,
                           Gpu::TextureLayout::ShaderReadOnlyOptimal,
                           textureHandle);

    return textureIndex;
}
void ShaderBindingContext::reset()
{
    allocator_.reset();

    nextTextureIndex_ = 0;
    nextBufferIndex_ = 0;
    nextSamplerIndex_ = 0;
}

} // namespace Cory