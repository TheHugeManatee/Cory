#pragma once

#include <Cory/Framegraph/Common.hpp>

#include <Cory/Base/Utils.hpp>
#include <Cory/Renderer/Common.hpp>
#include <Cory/Renderer/DescriptorSets.hpp>
#include <Cory/Renderer/GpuBumpAllocator.hpp>

#include <KDGpu/buffer.h>

#include <cstdint>
#include <span>
#include <unordered_map>
#include <utility>
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
    class ScopedBinding : NoCopy {
      public:
        ScopedBinding() = default;
        ScopedBinding(ShaderBindingContext &context, bool autoFlushOnExit);
        ~ScopedBinding();

        ScopedBinding(ScopedBinding &&rhs) noexcept;
        ScopedBinding &operator=(ScopedBinding &&rhs) noexcept;

        void flush();
        void release();

      private:
        ShaderBindingContext *context_{nullptr};
        bool autoFlushOnExit_{true};
    };

    explicit ShaderBindingContext(Gpu::Device &device,
                                  FramegraphResourceManager &resources,
                                  DescriptorSets &descriptorSets,
                                  uint32_t instanceIndex,
                                  size_t drawDataBufferSize);

    ~ShaderBindingContext();

    // clang-format off
    [[nodiscard]] TextureHeapIndex bindTexture2D(TransientTextureHandle textureHandle, Gpu::TextureLayout layout, Gpu::TextureSamplerHandle sampler = {});
    [[nodiscard]] TextureHeapIndex bindTexture2D(Gpu::TextureViewHandle view, Gpu::TextureLayout layout, Gpu::TextureSamplerHandle sampler = {});

    [[nodiscard]] TextureHeapIndex bindTexture3D(TransientTextureHandle textureHandle, Gpu::TextureLayout layout, Gpu::TextureSamplerHandle sampler = {});
    [[nodiscard]] TextureHeapIndex bindTexture3D(Gpu::TextureViewHandle view, Gpu::TextureLayout layout, Gpu::TextureSamplerHandle sampler = {});

    [[nodiscard]] TextureHeapIndex bindStorageImage2D(TransientTextureHandle textureHandle, Gpu::TextureLayout layout);
    [[nodiscard]] TextureHeapIndex bindStorageImage2D(Gpu::TextureViewHandle view, Gpu::TextureLayout layout);
    [[nodiscard]] TextureHeapIndex bindStorageImage2DMS(TransientTextureHandle textureHandle,
                                                        Gpu::TextureLayout layout);
    [[nodiscard]] TextureHeapIndex bindStorageImage2DMS(Gpu::TextureViewHandle view,
                                                        Gpu::TextureLayout layout);

    [[nodiscard]] TextureHeapIndex bindStorageImage3D(TransientTextureHandle textureHandle, Gpu::TextureLayout layout);
    [[nodiscard]] TextureHeapIndex bindStorageImage3D(Gpu::TextureViewHandle view, Gpu::TextureLayout layout);

    [[nodiscard]] SamplerHeapIndex bindSampler(Gpu::TextureSamplerHandle sampler);
    // clang-format on

    void bind(Gpu::RenderPassCommandRecorder &cmd);
    void bind(Gpu::RenderPassCommandRecorder &cmd, Gpu::PipelineLayoutHandle pipelineLayout);
    void bind(Gpu::ComputePassCommandRecorder &cmd);
    [[nodiscard]] ScopedBinding scoped(Gpu::RenderPassCommandRecorder &cmd,
                                       bool autoFlushOnExit = true);
    [[nodiscard]] ScopedBinding scoped(Gpu::RenderPassCommandRecorder &cmd,
                                       Gpu::PipelineLayoutHandle pipelineLayout,
                                       bool autoFlushOnExit = true);
    [[nodiscard]] ScopedBinding scoped(Gpu::ComputePassCommandRecorder &cmd,
                                       bool autoFlushOnExit = true);
    void unbind();

    /// Allocate a temp allocation in the per-draw data buffer
    template <typename T> GpuAllocation<T> alloc(size_t count = 1)
    // requires std::is_trivially_constructible_v<T>
    {
        return allocator_.alloc<T>(count);
    }

    /// Push a push constant value
    template <typename T>
    void push(const T &data)
        requires(sizeof(T) <= MAX_PUSH_CONSTANT_SIZE)
    {
        push(std::as_bytes(std::span{&data, size_t{1}}));
    }
    /// Push raw push constant data - usually the templated version should be preferred
    void push(std::span<const std::byte> data);

    /// Flush all writes to the bindings
    void flush();

    [[nodiscard]] size_t drawDataBytesUsed() const { return allocator_.usedBytes(); }
    [[nodiscard]] size_t drawDataBufferSize() const { return allocator_.capacityBytes(); }
    void resizeDrawDataBuffer(size_t drawDataBufferSize);

    /// Reset the binding context for a new frame
    void reset();

  private:
    struct TextureBindingKey {
        ImageBindPoint bindPoint{ImageBindPoint::Texture2D};
        Gpu::TextureViewHandle view{};
        Gpu::TextureLayout layout{Gpu::TextureLayout::Undefined};
        Gpu::TextureSamplerHandle sampler{};
        bool operator==(const TextureBindingKey &) const = default;
    };
    struct TextureBindingKeyHasher {
        std::size_t operator()(const TextureBindingKey &key) const noexcept
        {
            return hashCompose(key.bindPoint.value, key.view, key.layout, key.sampler);
        }
    };
    struct SamplerBindingKey {
        Gpu::TextureSamplerHandle sampler{};
        bool operator==(const SamplerBindingKey &) const = default;
    };
    struct SamplerBindingKeyHasher {
        std::size_t operator()(const SamplerBindingKey &key) const noexcept
        {
            return hashCompose(0, key.sampler);
        }
    };

    TextureHeapIndex &nextTextureIndex(ImageBindPoint bindPoint);

    TextureHeapIndex bindTexture(ImageBindPoint bindPoint,
                                 Gpu::TextureViewHandle view,
                                 Gpu::TextureLayout layout,
                                 Gpu::TextureSamplerHandle sampler);

    bool isDirty_{false};
    Gpu::Device *device_;
    std::unique_ptr<MappedCoherentDeviceBuffer> perDrawDataBuffer_;
    GpuBumpAllocator allocator_;
    FramegraphResourceManager *resources_;
    DescriptorSets *descriptorSets_;
    uint32_t instanceIndex_;

    std::variant<std::monostate, //
                 Gpu::RenderPassCommandRecorder *,
                 Gpu::ComputePassCommandRecorder *>
        passRecorder_;

    // Bump allocation indices
    TextureHeapIndex nextTexture2DIndex_{0};
    TextureHeapIndex nextTexture3DIndex_{0};
    TextureHeapIndex nextStorageImage2DIndex_{0};
    TextureHeapIndex nextStorageImage3DIndex_{0};
    TextureHeapIndex nextStorageImage2DMSIndex_{0};
    SamplerHeapIndex nextSamplerIndex_{0};

    std::unordered_map<TextureBindingKey, TextureHeapIndex, TextureBindingKeyHasher>
        textureBindingCache_;
    std::unordered_map<SamplerBindingKey, SamplerHeapIndex, SamplerBindingKeyHasher>
        samplerBindingCache_;
};
} // namespace Cory
