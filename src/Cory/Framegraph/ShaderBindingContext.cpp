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
    isDirty_ = true;
    return bindTexture(
        ImageBindPoint::Texture2D, resources_->imageView(textureHandle), layout, sampler);
}

TextureHeapIndex ShaderBindingContext::bindTexture2D(Gpu::TextureViewHandle view,
                                                     Gpu::TextureLayout layout,
                                                     Gpu::TextureSamplerHandle sampler)
{
    isDirty_ = true;
    return bindTexture(ImageBindPoint::Texture2D, view, layout, sampler);
}

TextureHeapIndex ShaderBindingContext::bindTexture3D(TransientTextureHandle textureHandle,
                                                     Gpu::TextureLayout layout,
                                                     Gpu::TextureSamplerHandle sampler)
{
    isDirty_ = true;
    CO_CORE_ASSERT(resources_ != nullptr, "ShaderBindingContext has no resource manager");
    return bindTexture(
        ImageBindPoint::Texture3D, resources_->imageView(textureHandle), layout, sampler);
}

TextureHeapIndex ShaderBindingContext::bindTexture3D(Gpu::TextureViewHandle view,
                                                     Gpu::TextureLayout layout,
                                                     Gpu::TextureSamplerHandle sampler)
{
    isDirty_ = true;
    return bindTexture(ImageBindPoint::Texture3D, view, layout, sampler);
}

TextureHeapIndex ShaderBindingContext::bindStorageImage2D(TransientTextureHandle textureHandle,
                                                          Gpu::TextureLayout layout)
{
    CO_CORE_ASSERT(resources_ != nullptr, "ShaderBindingContext has no resource manager");
    isDirty_ = true;
    return bindTexture(
        ImageBindPoint::StorageImage2D, resources_->imageView(textureHandle), layout, {});
}

TextureHeapIndex ShaderBindingContext::bindStorageImage2D(Gpu::TextureViewHandle view,
                                                          Gpu::TextureLayout layout)
{
    isDirty_ = true;
    return bindTexture(ImageBindPoint::StorageImage2D, view, layout, {});
}

TextureHeapIndex ShaderBindingContext::bindStorageImage3D(TransientTextureHandle textureHandle,
                                                          Gpu::TextureLayout layout)
{
    CO_CORE_ASSERT(resources_ != nullptr, "ShaderBindingContext has no resource manager");
    isDirty_ = true;
    return bindTexture(
        ImageBindPoint::StorageImage3D, resources_->imageView(textureHandle), layout, {});
}

TextureHeapIndex ShaderBindingContext::bindStorageImage3D(Gpu::TextureViewHandle view,
                                                          Gpu::TextureLayout layout)
{
    isDirty_ = true;
    return bindTexture(ImageBindPoint::StorageImage3D, view, layout, {});
}

SamplerHeapIndex ShaderBindingContext::bindSampler(Gpu::TextureSamplerHandle sampler)
{
    CO_CORE_DEBUG_ASSERT(descriptorSets_ != nullptr,
                         "ShaderBindingContext has no resource manager");
    isDirty_ = true;
    auto index = nextSamplerIndex_++;
    descriptorSets_->write(instanceIndex_, index, sampler);
    return index;
}

void ShaderBindingContext::bind(Gpu::RenderPassCommandRecorder &cmd)
{
    CO_CORE_ASSERT(std::holds_alternative<std::monostate>(passRecorder_),
                   "ShaderBindingContext is already bound to a pass recorder! Seems like a "
                   "previous pass did not unbind/end correctly!");
    passRecorder_ = &cmd;
    descriptorSets_->bind(cmd, instanceIndex_);
}

void ShaderBindingContext::bind(Gpu::RenderPassCommandRecorder &cmd,
                                Gpu::PipelineLayoutHandle pipelineLayout)
{
    CO_CORE_ASSERT(std::holds_alternative<std::monostate>(passRecorder_),
                   "ShaderBindingContext is already bound to a pass recorder! Seems like a "
                   "previous pass did not unbind/end correctly!");
    passRecorder_ = &cmd;
    descriptorSets_->bind(cmd, instanceIndex_, pipelineLayout);
}

void ShaderBindingContext::bind(Gpu::ComputePassCommandRecorder &cmd)
{
    CO_CORE_ASSERT(std::holds_alternative<std::monostate>(passRecorder_),
                   "ShaderBindingContext is already bound to a pass recorder! Seems like a "
                   "previous pass did not unbind/end correctly!");
    passRecorder_ = &cmd;
    descriptorSets_->bind(cmd, instanceIndex_);
}

void ShaderBindingContext::unbind()
{
    if (isDirty_) {
        CO_CORE_WARN("Unbind() called without a flush() after bindings were changed - you likely "
                     "forgot to flush() before drawing/dispatching!");
    }
    CO_CORE_ASSERT(!std::holds_alternative<std::monostate>(passRecorder_),
                   "Trying to unbind but there was no previous matched call to bind()!")
    passRecorder_ = std::monostate{};
}

void ShaderBindingContext::push(std::span<const std::byte> data)
{
    CO_CORE_DEBUG_ASSERT(data.size() <= MAX_PUSH_CONSTANT_SIZE,
                         "Push constant data exceeds maximum size of {} bytes",
                         MAX_PUSH_CONSTANT_SIZE);

    std::visit(lambda_visitor{[&](std::monostate) {
                                  CO_CORE_ASSERT(false,
                                                 "No active pass recorder to push constants to!");
                              },
                              [&](auto *recorder) {
                                  recorder->pushConstant(
                                      Gpu::PushConstantRange{
                                          .offset = 0,
                                          .size = gsl::narrow<uint32_t>(data.size()),
                                          .shaderStages = Gpu::ShaderStageFlagBits::All,
                                      },
                                      data.data());
                              }},
               passRecorder_);
}

void ShaderBindingContext::flush()
{
    descriptorSets_->flush(instanceIndex_);
    isDirty_ = false;
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
    }
    CO_CORE_ASSERT(false, "Unknown bind point {}", bindPoint.value);
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
    nextSamplerIndex_ = 0;
}

} // namespace Cory
