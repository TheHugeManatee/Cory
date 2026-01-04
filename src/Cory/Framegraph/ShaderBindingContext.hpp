#pragma once

#include <Cory/Framegraph/Common.hpp>

#include <Cory/Base/Utils.hpp>
#include <Cory/Framegraph/FramegraphResourceManager.hpp>
#include <Cory/Renderer/Common.hpp>
#include <Cory/Renderer/DescriptorSets.hpp>
#include <Cory/Renderer/GpuBumpAllocator.hpp>

#include <KDGpu/buffer.h>

#include <cstdint>
#include <span>
#include <variant>

namespace Cory {

/**
 * @brief Context for managing all bindings exposed to a shader in a bindless context
 *
 * This binding context manages:
 *  - Binding indices for resources (textures, buffers, samplers)
 *  - Temp allocations for coherently mapped buffers (replaces UBOs)
 *  - Push constants
 *
 * For textures:
 *  - For simplicity, we scrap the descriptor buffer contents every frame:
 *  - Whenever a pass requires a texture, it must already be uploaded.
 *  - The SBC then assigns it a binding index, and writes the descriptor set entry for it
 *  - texture assignment is also done via a simple bump allocator, hence each descriptor index
 *    will be used exactly once per frame
 *
 */
class ShaderBindingContext : NoCopy, NoMove {
  public:
    explicit ShaderBindingContext(Gpu::Device &device,
                                  FramegraphResourceManager &resources,
                                  DescriptorSets &descriptorSets,
                                  uint32_t instanceIndex,
                                  size_t drawDataBufferSize);

    ~ShaderBindingContext();

    template <typename T>
    GpuAllocation<T> alloc(size_t count = 1)
        requires std::is_trivially_constructible_v<T>
    {
        return allocator_.alloc<T>(count);
    }

    [[nodiscard]] TextureHeapIndex bindTexture2D(TransientTextureHandle textureHandle,
                                                 Gpu::TextureLayout layout,
                                                 Gpu::TextureSamplerHandle sampler = {});
    [[nodiscard]] TextureHeapIndex bindTexture2D(Gpu::TextureViewHandle view,
                                                 Gpu::TextureLayout layout,
                                                 Gpu::TextureSamplerHandle sampler = {});

    [[nodiscard]] TextureHeapIndex bindTexture3D(TransientTextureHandle textureHandle,
                                                 Gpu::TextureLayout layout,
                                                 Gpu::TextureSamplerHandle sampler = {});
    [[nodiscard]] TextureHeapIndex bindTexture3D(Gpu::TextureViewHandle view,
                                                 Gpu::TextureLayout layout,
                                                 Gpu::TextureSamplerHandle sampler = {});

    [[nodiscard]] TextureHeapIndex bindStorageImage2D(TransientTextureHandle textureHandle,
                                                      Gpu::TextureLayout layout);
    [[nodiscard]] TextureHeapIndex bindStorageImage2D(Gpu::TextureViewHandle view,
                                                      Gpu::TextureLayout layout);

    [[nodiscard]] TextureHeapIndex bindStorageImage3D(TransientTextureHandle textureHandle,
                                                      Gpu::TextureLayout layout);
    [[nodiscard]] TextureHeapIndex bindStorageImage3D(Gpu::TextureViewHandle view,
                                                      Gpu::TextureLayout layout);

    [[nodiscard]] SamplerHeapIndex bindSampler(Gpu::TextureSamplerHandle sampler);

    [[nodiscard]] BufferHeapIndex
    bindBuffer(TransientBufferHandle bufferHandle,
               BufferBindPoint bindPoint = BufferBindPoint::StorageBufferReadOnly);
    [[nodiscard]] BufferHeapIndex
    bindBuffer(Gpu::BufferHandle bufferHandle,
               BufferBindPoint bindPoint = BufferBindPoint::StorageBufferReadOnly);

    void bind(Gpu::RenderPassCommandRecorder &cmd);
    void bind(Gpu::RenderPassCommandRecorder &cmd, Gpu::PipelineLayoutHandle pipelineLayout);
    void bind(Gpu::ComputePassCommandRecorder &cmd);
    void unbind();

    /// Push a push constant value
    template <typename T>
    void push(const T &data)
        requires(sizeof(T) <= MAX_PUSH_CONSTANT_SIZE)
    {
        push(std::span{reinterpret_cast<const std::byte *>(&data), sizeof(data)});
    }
    /// Push raw push constant data - usually the templated version should be preferred
    void push(std::span<const std::byte> data);

    /// Flush all writes to the bindings
    void flush();

    /// Reset the binding context for a new frame
    void reset();

  private:
    TextureHeapIndex &nextTextureIndex(ImageBindPoint bindPoint);
    BufferHeapIndex &nextBufferIndex(BufferBindPoint bindPoint);

    TextureHeapIndex bindTexture(ImageBindPoint bindPoint,
                                 Gpu::TextureViewHandle view,
                                 Gpu::TextureLayout layout,
                                 Gpu::TextureSamplerHandle sampler);

    bool isDirty_{false};
    Gpu::Device *device_;
    Gpu::Buffer perDrawDataBuffer_;
    GpuBumpAllocator allocator_;
    FramegraphResourceManager *resources_;
    DescriptorSets *descriptorSets_;
    uint32_t instanceIndex_;

    std::variant<std::monostate,                   //
                 Gpu::RenderPassCommandRecorder *,
                 Gpu::ComputePassCommandRecorder *>
        passRecorder_;

    // Bump allocation indices
    TextureHeapIndex nextTexture2DIndex_{0};
    TextureHeapIndex nextTexture3DIndex_{0};
    TextureHeapIndex nextStorageImage2DIndex_{0};
    TextureHeapIndex nextStorageImage3DIndex_{0};
    BufferHeapIndex nextReadOnlyBufferIndex_{0};
    BufferHeapIndex nextReadWriteBufferIndex_{0};
    SamplerHeapIndex nextSamplerIndex_{0};
};
} // namespace Cory
