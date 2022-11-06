#include <Cory/Renderer/UniformBufferObject.hpp>

namespace Cory {

namespace {
size_t computeInstanceAlignment(Context &ctx, size_t instanceSize)
{
    auto uboAlignment =
        ctx.device().properties().properties().properties.limits.minUniformBufferOffsetAlignment;
    if (uboAlignment > 0) { return (instanceSize + uboAlignment - 1) & ~(uboAlignment - 1); }
    return instanceSize;
}
} // namespace

UniformBufferObjectBase::UniformBufferObjectBase(Context &ctx,
                                                 size_t instances,
                                                 size_t instanceSize)
    : ctx_{&ctx}
    , instanceSize_{instanceSize}
    , instanceAlignment_{computeInstanceAlignment(ctx, instanceSize)}
    , instances_{instances}
{
    size_t size = instances_ * instanceAlignment_;
    buffer_ = ctx_->resources().createBuffer(
        size, BufferUsageBits::UniformBuffer, MemoryFlagBits::HostVisible);

    // map the memory
    VkDeviceMemory memory = ctx_->resources()[buffer_].dedicatedMemory();
    VkResult r = ctx_->device()->MapMemory(
        ctx_->device(), memory, 0, VK_WHOLE_SIZE, 0, (void **)&mappedMemory_);
    THROW_ON_ERROR(r, "Mapping memory for uniform buffer failed");
}

UniformBufferObjectBase::~UniformBufferObjectBase()
{
    // unmap memory
    VkDeviceMemory memory = ctx_->resources()[buffer_].dedicatedMemory();
    ctx_->device()->UnmapMemory(ctx_->device(), memory);
    // release buffer
    ctx_->resources().release(buffer_);
}

void UniformBufferObjectBase::flushInternal() { flushInternal(0, instances_ * instanceAlignment_); }

void UniformBufferObjectBase::flushInternal(gsl::index instance)
{
    CO_CORE_ASSERT(instance < instances_, "Instance index out of range");
    flushInternal(instanceAlignment_ * instance, instanceAlignment_);
}

void UniformBufferObjectBase::flushInternal(VkDeviceSize offset, VkDeviceSize size)
{
    VkDeviceMemory memory = ctx_->resources()[buffer_].dedicatedMemory();
    VkMappedMemoryRange mappedRange = {.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                                       .memory = memory,
                                       .offset = offset,
                                       .size = size};
    auto r = ctx_->device()->FlushMappedMemoryRanges(ctx_->device(), 1, &mappedRange);
    THROW_ON_ERROR(r, "Error flushing UBO memory!");
}

std::byte *UniformBufferObjectBase::instanceAt(gsl::index instance)
{
    CO_CORE_ASSERT(instance < instances_, "Instance index out of range");
    return mappedMemory_ + instance * instanceAlignment_;
}

} // namespace Cory