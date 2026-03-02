#pragma once

#include <Cory/Base/Result.hpp>
#include <Cory/Renderer/Gpu.hpp>

#include <KDGpu/buffer.h>

#include <cppcoro/task.hpp>

#include <cstddef>

namespace Cory {

class ThreadScheduler;

struct StagingSlot {
    Gpu::Buffer buffer{};
    Gpu::DeviceSize byteSize{0};
    // AsyncUploader-provided mapped CPU pointer for the staging buffer.
    // Callers should treat this as opaque storage and must not free it.
    void *userData{nullptr};

    [[nodiscard]] bool valid() const noexcept { return buffer.isValid() || userData != nullptr; }
};

class IStagingUploader {
  public:
    virtual ~IStagingUploader() = default;

    [[nodiscard]] virtual cppcoro::task<Result<StagingSlot>>
    acquireStaging(Gpu::DeviceSize byteSize) = 0;
    [[nodiscard]] virtual ThreadScheduler *threadScheduler() const noexcept = 0;
    virtual void recycleStaging(StagingSlot &&stagingSlot) = 0;
    [[nodiscard]] virtual bool validStaging(const StagingSlot &stagingSlot) const = 0;
};

} // namespace Cory
