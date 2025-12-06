#include <Cory/Renderer/UniformBufferObject.hpp>

#include <numeric>

#include <Cory/Base/FmtUtils.hpp>
#include <Cory/Renderer/Context.hpp>

#include <KDGpu/buffer_options.h>
#include <KDGpu/vulkan/vulkan_resource_manager.h>

namespace Cory {

namespace {
size_t computeInstanceAlignment(Context &ctx, size_t instanceSize)
{
    const auto minOffsetAlignment = ctx.physicalDevice().limits.minUniformBufferOffsetAlignment;
    const auto atomSize = ctx.physicalDevice().limits.nonCoherentAtomSize;
    const auto alignment = std::lcm(minOffsetAlignment, atomSize);

    // round up to nearest multiple of alignment
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
    auto &device = ctx_->device();
    Gpu::DeviceSize size = instances_ * alignedInstanceSize_;
    bufferHandle_ = device.createBuffer(KDGpu::BufferOptions{
        .label = "Uniform Buffer",
        .size = size,
        .usage = Gpu::BufferUsageFlagBits::UniformBufferBit,
        .memoryUsage = KDGpu::MemoryUsage::CpuToGpu,
    });

    // Map as long as the UBO object lives
    mappedMemory_ =
        reinterpret_cast<std::byte *>(ctx_->resources().getBuffer(bufferHandle_)->map());
}

void swap(UniformBufferObjectBase &lhs, UniformBufferObjectBase &rhs) noexcept
{
    using std::swap;
    swap(lhs.ctx_, rhs.ctx_);
    swap(lhs.bufferHandle_, rhs.bufferHandle_);
    swap(lhs.mappedMemory_, rhs.mappedMemory_);
    swap(lhs.instanceSize_, rhs.instanceSize_);
    swap(lhs.alignedInstanceSize_, rhs.alignedInstanceSize_);
    swap(lhs.instances_, rhs.instances_);
}

UniformBufferObjectBase::UniformBufferObjectBase(UniformBufferObjectBase &&rhs) noexcept
    : UniformBufferObjectBase()
{
    swap(*this, rhs);
}

UniformBufferObjectBase &UniformBufferObjectBase::operator=(UniformBufferObjectBase &&rhs) noexcept
{
    swap(*this, rhs);
    return *this;
}

UniformBufferObjectBase::~UniformBufferObjectBase()
{
    if (auto *buffer = ctx_->resources().getBuffer(bufferHandle_); buffer != nullptr) {
        buffer->unmap();
        ctx_->resources().deleteBuffer(bufferHandle_);
    }
}

void UniformBufferObjectBase::flushInternal()
{
    auto *buffer = ctx_->resources().getBuffer(bufferHandle_);
    CO_CORE_ASSERT(buffer != nullptr, "UBO has invalid buffer handle!");

    buffer->flush();
}

std::byte *UniformBufferObjectBase::instanceAt(gsl::index instance)
{
    CO_CORE_ASSERT(instance < instances_, "Instance index out of range");
    return mappedMemory_ + instance * alignedInstanceSize_;
}
VkDescriptorBufferInfo UniformBufferObjectBase::descriptorInfo(gsl::index instance) const
{
    auto *buffer = ctx_->resources().getBuffer(bufferHandle_);
    CO_CORE_ASSERT(buffer != nullptr, "UBO has invalid buffer handle!");

    return VkDescriptorBufferInfo{.buffer = buffer->buffer,
                                  .offset = instance * alignedInstanceSize_,
                                  .range = alignedInstanceSize_};
}
} // namespace Cory