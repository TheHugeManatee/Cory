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
                                           FramegraphResourceManager &resources,
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
    , resources_(&resources)
    , descriptorSets_(&descriptorSets)
    , instanceIndex_(instanceIndex)
{
}

ShaderBindingContext::~ShaderBindingContext() {}

TextureHeapIndex ShaderBindingContext::bindTexture2D(TransientTextureHandle textureHandle,
                                                     Gpu::TextureLayout layout,
                                                     Gpu::TextureSamplerHandle sampler)
{
    CO_CORE_ASSERT(resources_ != nullptr, "ShaderBindingContext has no resource manager");
    return bindTexture(ImageBindPoint::Texture2D,
                       resources_->imageView(textureHandle),
                       layout,
                       sampler);
}

TextureHeapIndex ShaderBindingContext::bindTexture2D(Gpu::TextureViewHandle view,
                                                     Gpu::TextureLayout layout,
                                                     Gpu::TextureSamplerHandle sampler)
{
    return bindTexture(ImageBindPoint::Texture2D, view, layout, sampler);
}

TextureHeapIndex ShaderBindingContext::bindTexture3D(TransientTextureHandle textureHandle,
                                                     Gpu::TextureLayout layout,
                                                     Gpu::TextureSamplerHandle sampler)
{
    CO_CORE_ASSERT(resources_ != nullptr, "ShaderBindingContext has no resource manager");
    return bindTexture(ImageBindPoint::Texture3D,
                       resources_->imageView(textureHandle),
                       layout,
                       sampler);
}

TextureHeapIndex ShaderBindingContext::bindTexture3D(Gpu::TextureViewHandle view,
                                                     Gpu::TextureLayout layout,
                                                     Gpu::TextureSamplerHandle sampler)
{
    return bindTexture(ImageBindPoint::Texture3D, view, layout, sampler);
}

TextureHeapIndex ShaderBindingContext::bindStorageImage2D(TransientTextureHandle textureHandle,
                                                          Gpu::TextureLayout layout)
{
    CO_CORE_ASSERT(resources_ != nullptr, "ShaderBindingContext has no resource manager");
    return bindTexture(ImageBindPoint::StorageImage2D,
                       resources_->imageView(textureHandle),
                       layout,
                       {});
}

TextureHeapIndex ShaderBindingContext::bindStorageImage2D(Gpu::TextureViewHandle view,
                                                          Gpu::TextureLayout layout)
{
    return bindTexture(ImageBindPoint::StorageImage2D, view, layout, {});
}

TextureHeapIndex ShaderBindingContext::bindStorageImage3D(TransientTextureHandle textureHandle,
                                                          Gpu::TextureLayout layout)
{
    CO_CORE_ASSERT(resources_ != nullptr, "ShaderBindingContext has no resource manager");
    return bindTexture(ImageBindPoint::StorageImage3D,
                       resources_->imageView(textureHandle),
                       layout,
                       {});
}

TextureHeapIndex ShaderBindingContext::bindStorageImage3D(Gpu::TextureViewHandle view,
                                                          Gpu::TextureLayout layout)
{
    return bindTexture(ImageBindPoint::StorageImage3D, view, layout, {});
}

SamplerHeapIndex ShaderBindingContext::bindSampler(Gpu::TextureSamplerHandle sampler)
{
    auto index = nextSamplerIndex_++;
    descriptorSets_->write(instanceIndex_, index, sampler);
    return index;
}

BufferHeapIndex ShaderBindingContext::bindBuffer(TransientBufferHandle bufferHandle,
                                                 BufferBindPoint bindPoint)
{
    CO_CORE_ASSERT(resources_ != nullptr, "ShaderBindingContext has no resource manager");
    return bindBuffer(resources_->buffer(bufferHandle), bindPoint);
}

BufferHeapIndex ShaderBindingContext::bindBuffer(Gpu::BufferHandle bufferHandle,
                                                 BufferBindPoint bindPoint)
{
    auto &nextIndex = nextBufferIndex(bindPoint);
    const BufferHeapIndex index = nextIndex++;
    descriptorSets_->write(bindPoint, instanceIndex_, index, bufferHandle);
    return index;
}

void ShaderBindingContext::bind(Gpu::RenderPassCommandRecorder &cmd)
{
    descriptorSets_->bind(cmd, instanceIndex_);
}

void ShaderBindingContext::bind(Gpu::RenderPassCommandRecorder &cmd,
                                Gpu::PipelineLayoutHandle pipelineLayout)
{
    descriptorSets_->bind(cmd, instanceIndex_, pipelineLayout);
}

void ShaderBindingContext::bind(Gpu::ComputePassCommandRecorder &cmd)
{
    descriptorSets_->bind(cmd, instanceIndex_);
}

TextureHeapIndex &ShaderBindingContext::nextTextureIndex(ImageBindPoint bindPoint)
{
    switch (bindPoint) {
        case ImageBindPoint::Texture2D:
            return nextTexture2DIndex_;
        case ImageBindPoint::Texture3D:
            return nextTexture3DIndex_;
        case ImageBindPoint::StorageImage2D:
            return nextStorageImage2DIndex_;
        case ImageBindPoint::StorageImage3D:
            return nextStorageImage3DIndex_;
        case ImageBindPoint::Samplers:
            CO_CORE_ASSERT(false, "Sampler bindings should use bindSampler");
            break;
    }
    return nextTexture2DIndex_;
}

BufferHeapIndex &ShaderBindingContext::nextBufferIndex(BufferBindPoint bindPoint)
{
    switch (bindPoint.value) {
        case BufferBindPoint::StorageBufferReadOnly:
            return nextReadOnlyBufferIndex_;
        case BufferBindPoint::StorageBufferReadWrite:
            return nextReadWriteBufferIndex_;
        default:
            CO_CORE_ASSERT(false, "Unknown buffer bind point");
            break;
    }
    return nextReadOnlyBufferIndex_;
}

TextureHeapIndex ShaderBindingContext::bindTexture(ImageBindPoint bindPoint,
                                                   Gpu::TextureViewHandle view,
                                                   Gpu::TextureLayout layout,
                                                   Gpu::TextureSamplerHandle sampler)
{
    auto &nextIndex = nextTextureIndex(bindPoint);
    const TextureHeapIndex index = nextIndex++;
    descriptorSets_->write(bindPoint, instanceIndex_, index, layout, view, sampler);
    return index;
}

void ShaderBindingContext::reset()
{
    allocator_.reset();

    nextTexture2DIndex_ = 0;
    nextTexture3DIndex_ = 0;
    nextStorageImage2DIndex_ = 0;
    nextStorageImage3DIndex_ = 0;
    nextReadOnlyBufferIndex_ = 0;
    nextReadWriteBufferIndex_ = 0;
    nextSamplerIndex_ = 0;
}

} // namespace Cory
