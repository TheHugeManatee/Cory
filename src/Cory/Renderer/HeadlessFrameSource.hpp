#pragma once

#include <Cory/Base/Common.hpp>
#include <Cory/Renderer/Common.hpp>
#include <Cory/Renderer/FrameGenerator.hpp>
#include <Cory/Renderer/FrameSource.hpp>
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

class HeadlessFrameSource : public FrameSource, NoCopy, NoMove {
  public:
    HeadlessFrameSource(Context &context, HeadlessFrameSourceCreateInfo createInfo);
    ~HeadlessFrameSource() override;

    [[nodiscard]] FrameGenerator frames() override;

    [[nodiscard]] Gpu::Format colorFormat() const noexcept override;
    [[nodiscard]] Gpu::Format depthFormat() const noexcept override;
    [[nodiscard]] glm::u32vec2 extent() const noexcept override;
    [[nodiscard]] Gpu::SampleCountFlagBits sampleCount() const noexcept override;
    [[nodiscard]] size_t size() const noexcept override;

  private:
    cppcoro::generator<FrameContext> frameGenerator();
    void submit(FrameContext &frameCtx);

    friend struct HeadlessFrameSourcePrivate;
    std::unique_ptr<HeadlessFrameSourcePrivate> data_;
};

} // namespace Cory
