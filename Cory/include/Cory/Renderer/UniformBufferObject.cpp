#include <Cory/Renderer/UniformBufferObject.hpp>

#include <numeric>

#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/ResourceManager.hpp>

#include <Magnum/Vk/Buffer.h>
#include <Magnum/Vk/Device.h>
#include <Magnum/Vk/DeviceProperties.h>
#include <Magnum/Vk/Vulkan.h>

namespace Cory {

namespace {
size_t computeInstanceAlignment(Context &ctx, size_t instanceSize)
{
    const auto minOffsetAlignment =
        ctx.device().properties().properties().properties.limits.minUniformBufferOffsetAlignment;
    const auto atomSize =
        ctx.device().properties().properties().properties.limits.nonCoherentAtomSize;

    const auto alignment = std::lcm(minOffsetAlignment, atomSize);

    if (alignment > 0) { return (instanceSize + alignment - 1) & ~(alignment - 1); }
    return instanceSize;
}
} // namespace

UniformBufferObjectBase::UniformBufferObjectBase(Context &ctx,
                                                 size_t instances,
                                                 size_t instanceSize)
    : ctx_{&ctx}
    , instanceSize_{instanceSize}
    , alignedInstanceSize_{computeInstanceAlignment(ctx, instanceSize)}
    , instances_{instances}
{
    size_t size = instances_ * alignedInstanceSize_;
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

void UniformBufferObjectBase::flushInternal()
{
    flushInternal(0, instances_ * alignedInstanceSize_);
}

void UniformBufferObjectBase::flushInternal(gsl::index instance)
{
    CO_CORE_ASSERT(instance < instances_, "Instance index out of range");
    flushInternal(alignedInstanceSize_ * instance, alignedInstanceSize_);
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
    return mappedMemory_ + instance * alignedInstanceSize_;
}
VkDescriptorBufferInfo UniformBufferObjectBase::descriptorInfo(gsl::index instance)
{
    return VkDescriptorBufferInfo{.buffer = ctx_->resources()[buffer_].handle(),
                                  .offset = instance * alignedInstanceSize_,
                                  .range = alignedInstanceSize_};
}

} // namespace Cory