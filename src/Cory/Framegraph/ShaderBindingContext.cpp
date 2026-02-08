#include "ShaderBindingContext.hpp"

#include <Cory/Base/Log.hpp>
#include <Cory/Framegraph/FramegraphResourceManager.hpp>
#include <Cory/Renderer/DescriptorSets.hpp>
#include <Cory/Renderer/MappedCoherentDeviceBuffer.hpp>

#include <KDGpu/buffer_options.h>
#include <KDGpu/device.h>
#include <KDGpu/vulkan/vulkan_graphics_api.h>

namespace Cory {

namespace {
std::unique_ptr<MappedCoherentDeviceBuffer> createPerDrawDataBuffer(Gpu::Device &device,
                                                                    size_t size)
{
    return std::make_unique<MappedCoherentDeviceBuffer>(
        device,
        MappedCoherentDeviceBufferCreateInfo{
            .label = "ShaderBindingContext Per-Draw Data Buffer",
            .size = size,
            .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,

        });
}
} // namespace

ShaderBindingContext::ScopedBinding::ScopedBinding(ShaderBindingContext &context,
                                                   bool autoFlushOnExit)
    : context_{&context}
    , autoFlushOnExit_{autoFlushOnExit}
{
}

ShaderBindingContext::ScopedBinding::~ScopedBinding()
{
    release();
}

ShaderBindingContext::ScopedBinding::ScopedBinding(ScopedBinding &&rhs) noexcept
    : context_{std::exchange(rhs.context_, nullptr)}
    , autoFlushOnExit_{rhs.autoFlushOnExit_}
{
}

ShaderBindingContext::ScopedBinding &
ShaderBindingContext::ScopedBinding::operator=(ScopedBinding &&rhs) noexcept
{
    if (this == &rhs) {
        return *this;
    }
    release();
    context_ = std::exchange(rhs.context_, nullptr);
    autoFlushOnExit_ = rhs.autoFlushOnExit_;
    return *this;
}

void ShaderBindingContext::ScopedBinding::flush()
{
    if (context_ != nullptr) {
        context_->flush();
    }
}

void ShaderBindingContext::ScopedBinding::release()
{
    if (context_ == nullptr) {
        return;
    }
    if (autoFlushOnExit_ && context_->isDirty_) {
        context_->flush();
    }
    context_->unbind();
    context_ = nullptr;
}

ShaderBindingContext::ShaderBindingContext(Gpu::Device &device,
                                           FramegraphResourceManager &resources,
                                           DescriptorSets &descriptorSets,
                                           uint32_t instanceIndex,
                                           size_t drawDataBufferSize)
    : device_{&device}
    , perDrawDataBuffer_(createPerDrawDataBuffer(device, drawDataBufferSize))
    , allocator_(GpuAllocation{perDrawDataBuffer_->allocation()})
    , resources_(&resources)
    , descriptorSets_(&descriptorSets)
    , instanceIndex_(instanceIndex)
{
    CO_CORE_ASSERT(perDrawDataBuffer_ != nullptr && perDrawDataBuffer_->isValid(),
                   "ShaderBindingContext: per-draw data buffer creation failed.");
}

ShaderBindingContext::~ShaderBindingContext() {}

TextureHeapIndex ShaderBindingContext::bindTexture2D(TransientTextureHandle textureHandle,
                                                     Gpu::TextureLayout layout,
                                                     Gpu::TextureSamplerHandle sampler)
{
    CO_CORE_ASSERT(resources_ != nullptr, "ShaderBindingContext has no resource manager");
    return bindTexture(
        ImageBindPoint::Texture2D, resources_->imageView(textureHandle), layout, sampler);
}

TextureHeapIndex ShaderBindingContext::bindTexture2D(Gpu::TextureViewHandle view,
                                                     Gpu::TextureLayout layout,
                                                     Gpu::TextureSamplerHandle sampler)
{
    CO_CORE_ASSERT(resources_ != nullptr, "ShaderBindingContext has no resource manager");
    return bindTexture(ImageBindPoint::Texture2D, view, layout, sampler);
}

TextureHeapIndex ShaderBindingContext::bindTexture3D(TransientTextureHandle textureHandle,
                                                     Gpu::TextureLayout layout,
                                                     Gpu::TextureSamplerHandle sampler)
{
    CO_CORE_ASSERT(resources_ != nullptr, "ShaderBindingContext has no resource manager");
    return bindTexture(
        ImageBindPoint::Texture3D, resources_->imageView(textureHandle), layout, sampler);
}

TextureHeapIndex ShaderBindingContext::bindTexture3D(Gpu::TextureViewHandle view,
                                                     Gpu::TextureLayout layout,
                                                     Gpu::TextureSamplerHandle sampler)
{
    CO_CORE_ASSERT(resources_ != nullptr, "ShaderBindingContext has no resource manager");
    return bindTexture(ImageBindPoint::Texture3D, view, layout, sampler);
}

TextureHeapIndex ShaderBindingContext::bindStorageImage2D(TransientTextureHandle textureHandle,
                                                          Gpu::TextureLayout layout)
{
    CO_CORE_ASSERT(resources_ != nullptr, "ShaderBindingContext has no resource manager");
    return bindTexture(
        ImageBindPoint::StorageImage2D, resources_->imageView(textureHandle), layout, {});
}

TextureHeapIndex ShaderBindingContext::bindStorageImage2D(Gpu::TextureViewHandle view,
                                                          Gpu::TextureLayout layout)
{
    CO_CORE_ASSERT(resources_ != nullptr, "ShaderBindingContext has no resource manager");
    return bindTexture(ImageBindPoint::StorageImage2D, view, layout, {});
}

TextureHeapIndex ShaderBindingContext::bindStorageImage2DMS(TransientTextureHandle textureHandle,
                                                            Gpu::TextureLayout layout)
{
    CO_CORE_ASSERT(resources_ != nullptr, "ShaderBindingContext has no resource manager");
    return bindTexture(
        ImageBindPoint::StorageImage2DMS, resources_->imageView(textureHandle), layout, {});
}

TextureHeapIndex ShaderBindingContext::bindStorageImage2DMS(Gpu::TextureViewHandle view,
                                                            Gpu::TextureLayout layout)
{
    CO_CORE_ASSERT(resources_ != nullptr, "ShaderBindingContext has no resource manager");
    return bindTexture(ImageBindPoint::StorageImage2DMS, view, layout, {});
}

TextureHeapIndex ShaderBindingContext::bindStorageImage3D(TransientTextureHandle textureHandle,
                                                          Gpu::TextureLayout layout)
{
    CO_CORE_ASSERT(resources_ != nullptr, "ShaderBindingContext has no resource manager");
    return bindTexture(
        ImageBindPoint::StorageImage3D, resources_->imageView(textureHandle), layout, {});
}

TextureHeapIndex ShaderBindingContext::bindStorageImage3D(Gpu::TextureViewHandle view,
                                                          Gpu::TextureLayout layout)
{
    CO_CORE_ASSERT(resources_ != nullptr, "ShaderBindingContext has no resource manager");
    return bindTexture(ImageBindPoint::StorageImage3D, view, layout, {});
}

SamplerHeapIndex ShaderBindingContext::bindSampler(Gpu::TextureSamplerHandle sampler)
{
    CO_CORE_DEBUG_ASSERT(descriptorSets_ != nullptr,
                         "ShaderBindingContext has no resource manager");
    const SamplerBindingKey key{.sampler = sampler};
    if (auto it = samplerBindingCache_.find(key); it != samplerBindingCache_.end()) {
        return it->second;
    }

    const auto index = nextSamplerIndex_++;
    descriptorSets_->write(instanceIndex_, index, sampler);
    samplerBindingCache_.emplace(key, index);
    isDirty_ = true;
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

ShaderBindingContext::ScopedBinding
ShaderBindingContext::scoped(Gpu::RenderPassCommandRecorder &cmd, bool autoFlushOnExit)
{
    bind(cmd);
    return ScopedBinding{*this, autoFlushOnExit};
}

ShaderBindingContext::ScopedBinding
ShaderBindingContext::scoped(Gpu::RenderPassCommandRecorder &cmd,
                             Gpu::PipelineLayoutHandle pipelineLayout,
                             bool autoFlushOnExit)
{
    bind(cmd, pipelineLayout);
    return ScopedBinding{*this, autoFlushOnExit};
}

ShaderBindingContext::ScopedBinding
ShaderBindingContext::scoped(Gpu::ComputePassCommandRecorder &cmd, bool autoFlushOnExit)
{
    bind(cmd);
    return ScopedBinding{*this, autoFlushOnExit};
}

void ShaderBindingContext::unbind()
{
    if (isDirty_) {
        CO_CORE_WARN("Unbind() called without a flush() after bindings were changed - you likely "
                     "forgot to flush() before drawing/dispatching!");
    }
    CO_CORE_ASSERT(!std::holds_alternative<std::monostate>(passRecorder_),
                   "Trying to unbind but there was no previous matched call to bind()!");
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

void ShaderBindingContext::resizeDrawDataBuffer(size_t drawDataBufferSize)
{
    CO_CORE_ASSERT(drawDataBufferSize > 0,
                   "ShaderBindingContext: drawDataBufferSize must be larger than zero.");
    if (drawDataBufferSize == perDrawDataBuffer_->allocation().size) {
        return;
    }

    perDrawDataBuffer_ = createPerDrawDataBuffer(*device_, drawDataBufferSize);
    CO_CORE_ASSERT(perDrawDataBuffer_ != nullptr && perDrawDataBuffer_->isValid(),
                   "ShaderBindingContext: per-draw data buffer recreation failed.");
    allocator_ = GpuBumpAllocator{GpuAllocation{perDrawDataBuffer_->allocation()}};
    reset();
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
    case ImageBindPoint::StorageImage2DMS:
        return nextStorageImage2DMSIndex_;
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
    const TextureBindingKey key{
        .bindPoint = bindPoint,
        .view = view,
        .layout = layout,
        .sampler = sampler,
    };
    if (auto it = textureBindingCache_.find(key); it != textureBindingCache_.end()) {
        return it->second;
    }

    auto &nextIndex = nextTextureIndex(bindPoint);
    const TextureHeapIndex index = nextIndex++;
    descriptorSets_->write(bindPoint, instanceIndex_, index, layout, view, sampler);
    textureBindingCache_.emplace(key, index);
    isDirty_ = true;
    return index;
}

void ShaderBindingContext::reset()
{
    allocator_.reset();

    nextTexture2DIndex_ = 0;
    nextTexture3DIndex_ = 0;
    nextStorageImage2DIndex_ = 0;
    nextStorageImage3DIndex_ = 0;
    nextStorageImage2DMSIndex_ = 0;
    nextSamplerIndex_ = 0;
    textureBindingCache_.clear();
    samplerBindingCache_.clear();
}

} // namespace Cory
