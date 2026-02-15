#pragma once

#include <Cory/Base/Common.hpp>
#include <Cory/Renderer/FrameGenerator.hpp>
#include <Cory/Renderer/Gpu.hpp>

#include <glm/vec2.hpp>

#include <cstddef>

namespace Cory {

/**
 * @brief Unified frame lifecycle source used by windowed and headless render paths.
 */
class FrameSource {
  public:
    virtual ~FrameSource() = default;

    [[nodiscard]] virtual FrameGenerator frames() = 0;
    [[nodiscard]] virtual Gpu::Format colorFormat() const noexcept = 0;
    [[nodiscard]] virtual Gpu::Format depthFormat() const noexcept = 0;
    [[nodiscard]] virtual glm::u32vec2 extent() const noexcept = 0;
    [[nodiscard]] virtual Gpu::SampleCountFlagBits sampleCount() const noexcept = 0;
    [[nodiscard]] virtual size_t size() const noexcept = 0;
};

} // namespace Cory
