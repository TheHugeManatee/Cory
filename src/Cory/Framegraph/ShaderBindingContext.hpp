#pragma once

#include <Cory/Renderer/Common.hpp>
#include <Cory/Renderer/GpuBumpAllocator.hpp>

#include <KDGpu/buffer.h>

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
                                  DescriptorSets &descriptorSets,
                                  uint32_t instanceIndex,
                                  size_t drawDataBufferSize);

    ~ShaderBindingContext();

    template<typename T>
    GpuAllocation<T> alloc(size_t count = 1)
        requires std::is_trivially_constructible_v<T>
    {
        return allocator_.alloc<T>(count);
    }

    TextureHeapIndex bindTexture2D(Gpu::TextureViewHandle textureHandle);

    void reset();

  private:
    Gpu::Device* device_;
    Gpu::Buffer perDrawDataBuffer_;
    GpuBumpAllocator allocator_;
    DescriptorSets *descriptorSets_;
    uint32_t instanceIndex_;

    // Bump allocation indices
    TextureHeapIndex nextTextureIndex_{0};
    BufferHeapIndex nextBufferIndex_{0};
    SamplerHeapIndex nextSamplerIndex_{0};
};
} // namespace Cory