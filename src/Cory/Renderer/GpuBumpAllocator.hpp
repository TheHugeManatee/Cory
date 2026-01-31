#pragma once

#include <Cory/Renderer/Common.hpp>

#include <cstddef>

namespace Cory {

/**
 * @brief Simple GPU bump allocator for linear allocations within a pre-allocated buffer
 *
 * This allocator manages a single contiguous allocation on the GPU, and provides simple
 * bump allocation within that region.
 */
class GpuBumpAllocator {
  public:
    explicit GpuBumpAllocator(GpuAllocation<std::byte> baseAllocation);

    constexpr size_t alignRoundUp(size_t offset, size_t alignment)
    {
        return (offset + alignment - 1) & ~(alignment - 1);
    }

    GpuAllocation<std::byte> alloc(size_t bytes, size_t align = 16);

    template <typename T> GpuAllocation<T> alloc(size_t count = 1)
    // requires std::is_trivially_constructible_v<T>
    {
        GpuAllocation<std::byte> mem = alloc(sizeof(T) * count, alignof(T));
        return GpuAllocation<T>{
            .cpu = reinterpret_cast<T *>(mem.cpu),
            .gpu = mem.gpu,
            .size = mem.size / sizeof(T),
        };
    }

    void reset() { offset_ = 0; }

  private:
    GpuAllocation<std::byte> baseAllocation_;
    size_t offset_{0};
};

} // namespace Cory
