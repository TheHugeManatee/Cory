#pragma once

#include <Cory/Renderer/Common.hpp>

#include <KDGpu/buffer.h>

#include <concepts>

namespace Cory {

/// lower-level UBO wrapper - not to be used directly, use UniformBufferObject<...> instead!
class UniformBufferObjectBase : NoCopy {
  public:
    Gpu::BufferHandle handle() const noexcept { return buffer_.handle(); }
    const Gpu::Buffer &buffer() const noexcept { return buffer_; }
    size_t instances() const noexcept { return instances_; }

  protected:
    UniformBufferObjectBase(Context &ctx, size_t instances, size_t instanceSize);
    ~UniformBufferObjectBase();

    friend void swap(UniformBufferObjectBase &lhs, UniformBufferObjectBase &rhs) noexcept;

    // movable
    UniformBufferObjectBase(UniformBufferObjectBase &&) noexcept;
    UniformBufferObjectBase &operator=(UniformBufferObjectBase &&) noexcept;

    // flush the whole buffer
    void flushInternal();
    // get pointer to aligned instance
    std::byte *instanceAt(gsl::index instance);

  private:
    UniformBufferObjectBase() = default; // private, only used for swap
    Context *ctx_{};
    Gpu::Buffer buffer_{};
    std::byte *mappedMemory_{nullptr};
    size_t instanceSize_{};
    size_t alignedInstanceSize_{};
    size_t instances_{};
};

/**
 * wrapper for a set of uniform buffer instances.
 *
 * typically, one instance per frame in flight is used.
 * @tparam BufferStruct
 */
template <typename BufferStruct>
    requires std::is_trivial_v<BufferStruct> // not sure what would happen to non-trivial types..
class UniformBufferObject : public UniformBufferObjectBase {
  public:
    UniformBufferObject(Context &ctx, size_t instances)
        : UniformBufferObjectBase(ctx, instances, sizeof(BufferStruct))
    {
    }

    // movable
    UniformBufferObject(UniformBufferObject &&) = default;
    UniformBufferObject &operator=(UniformBufferObject &&) = default;

    /**
     * Access an instance at a specific index
     *
     * @note you will have to manually flush after you have modified the memory
     */
    BufferStruct &operator[](gsl::index instance)
    {
        return *reinterpret_cast<BufferStruct *>(instanceAt(instance));
    }

    /// flush a specific instance to make it available on the GPU
    void flush(gsl::index instance) { UniformBufferObjectBase::flushInternal(); }

    /// update the cpu data and flush it to the GPU
    void writeAndFlush(gsl::index instance, const BufferStruct &data)
    {
        *reinterpret_cast<BufferStruct *>(instanceAt(instance)) = data;
        flush(instance);
    }

  private:
};

} // namespace Cory
