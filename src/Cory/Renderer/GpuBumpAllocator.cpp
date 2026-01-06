#include "GpuBumpAllocator.hpp"

#include <Cory/Base/Log.hpp>

namespace Cory {

GpuBumpAllocator::GpuBumpAllocator(GpuAllocation<uint8_t> baseAllocation)
    : baseAllocation_(baseAllocation)
{
}

GpuAllocation<uint8_t> GpuBumpAllocator::alloc(size_t bytes, size_t align)
{
    CO_CORE_DEBUG_ASSERT(align && (align & (align - 1)) == 0, "Alignment must be a power of two");
    CO_CORE_ASSERT(bytes <= baseAllocation_.size, "Allocation size exceeds total allocation size");
    CO_CORE_DEBUG_ASSERT(reinterpret_cast<uintptr_t>(baseAllocation_.cpu) % align == 0,
                         "Base allocation is not aligned to the requested alignment");

    offset_ = alignRoundUp(offset_, align);
    if (offset_ + bytes > baseAllocation_.size) {
        CO_CORE_ERROR("GpuBumpAllocator: Out of memory (requested {}, available {})",
                      bytes,
                      baseAllocation_.size - offset_);
        throw std::bad_alloc();
    }

    GpuAllocation alloc = {
        .cpu = baseAllocation_.cpu + offset_,
        .gpu = baseAllocation_.gpu + offset_,
        .size = bytes,
    };

    offset_ += bytes;
    return alloc;
}
} // namespace Cory