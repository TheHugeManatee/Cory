#pragma once

#include <Cory/Base/Common.hpp>
#include <Cory/Renderer/Common.hpp>
#include <Cory/Renderer/FrameGenerator.hpp>
#include <Cory/Renderer/Gpu.hpp>

#include <glm/vec2.hpp>

#include <cstddef>
#include <memory>
#include <string>

namespace Cory {

struct HeadlessFrameSourcePrivate;

struct HeadlessFrameSourceCreateInfo {
    std::string label{"HeadlessFrameSource"};
    glm::u32vec2 size{1024, 768};
    Gpu::SampleCountFlagBits samples{Gpu::SampleCountFlagBits::Samples1Bit};
    Gpu::Format colorFormat{Gpu::Format::B8G8R8A8_UNORM};
    size_t imageCount{MAX_FRAMES_IN_FLIGHT};
};

class Context;

class HeadlessFrameSource : NoCopy, NoMove {
  public:
    HeadlessFrameSource(Context &context, HeadlessFrameSourceCreateInfo createInfo);
    ~HeadlessFrameSource();

    [[nodiscard]] FrameGenerator frames();

    [[nodiscard]] Gpu::Format colorFormat() const noexcept;
    [[nodiscard]] Gpu::Format depthFormat() const noexcept;
    [[nodiscard]] glm::u32vec2 extent() const noexcept;
    [[nodiscard]] Gpu::SampleCountFlagBits sampleCount() const noexcept;
    [[nodiscard]] size_t size() const noexcept;

  private:
    cppcoro::generator<FrameContext> frameGenerator();
    void submit(FrameContext &frameCtx);

    friend struct HeadlessFrameSourcePrivate;
    std::unique_ptr<HeadlessFrameSourcePrivate> data_;
};

} // namespace Cory
