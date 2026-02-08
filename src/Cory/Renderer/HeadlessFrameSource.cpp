#include <Cory/Renderer/HeadlessFrameSource.hpp>

#include <Cory/Base/FmtUtils.hpp>
#include <Cory/Base/GlmUtils.hpp>
#include <Cory/Base/Log.hpp>
#include <Cory/Base/Profiling.hpp>
#include <Cory/Renderer/Context.hpp>

#include <KDGpu/texture.h>
#include <KDGpu/texture_options.h>
#include <KDGpuUtils/resource_deleter.h>

#include <range/v3/range/conversion.hpp>
#include <range/v3/view/enumerate.hpp>
#include <range/v3/view/indices.hpp>
#include <range/v3/view/transform.hpp>

#include <magic_enum/magic_enum.hpp>

#include <array>
#include <optional>

namespace Cory {

namespace {

Gpu::Format selectDepthFormat(const Gpu::Adapter &adapter)
{
    constexpr std::array preferredDepthFormat = {
        Gpu::Format::D24_UNORM_S8_UINT,
        Gpu::Format::D16_UNORM_S8_UINT,
        Gpu::Format::D32_SFLOAT_S8_UINT,
        Gpu::Format::D16_UNORM,
        Gpu::Format::D32_SFLOAT,
    };
    for (const auto &depthFormat : preferredDepthFormat) {
        const auto formatProperties = adapter.formatProperties(depthFormat);
        if (formatProperties.optimalTilingFeatures &
            Gpu::FormatFeatureFlagBit::DepthStencilAttachmentBit) {
            return depthFormat;
        }
    }
    return Gpu::Format::D24_UNORM_S8_UINT;
}

Gpu::SampleCountFlagBits clampSampleCountToSupported(const Gpu::Adapter &adapter,
                                                     Gpu::SampleCountFlagBits requested)
{
    const auto &limits = adapter.properties().limits;
    const auto supportedMask = static_cast<uint32_t>(limits.framebufferColorSampleCounts.toInt() &
                                                     limits.framebufferDepthSampleCounts.toInt());

    constexpr std::array preferred = {
        Gpu::SampleCountFlagBits::Samples64Bit,
        Gpu::SampleCountFlagBits::Samples32Bit,
        Gpu::SampleCountFlagBits::Samples16Bit,
        Gpu::SampleCountFlagBits::Samples8Bit,
        Gpu::SampleCountFlagBits::Samples4Bit,
        Gpu::SampleCountFlagBits::Samples2Bit,
        Gpu::SampleCountFlagBits::Samples1Bit,
    };

    const auto requestedBits = static_cast<uint32_t>(requested);
    for (const auto sample : preferred) {
        const auto sampleBits = static_cast<uint32_t>(sample);
        if (sampleBits <= requestedBits && (supportedMask & sampleBits) != 0U) {
            return sample;
        }
    }
    return Gpu::SampleCountFlagBits::Samples1Bit;
}

} // namespace

struct HeadlessFrameSourcePrivate {
    Context *ctx{};
    std::string label;
    glm::u32vec2 extent{};
    Gpu::SampleCountFlagBits sampleCount{Gpu::SampleCountFlagBits::Samples1Bit};
    Gpu::Format colorFormat{Gpu::Format::B8G8R8A8_UNORM};
    Gpu::Format depthFormat{Gpu::Format::D24_UNORM_S8_UINT};
    size_t imageCount{MAX_FRAMES_IN_FLIGHT};

    uint64_t frameNumber{0};

    std::vector<Texture> swapchainImages;
    std::vector<TextureView> swapchainViews;

    std::vector<Texture> colorImages;
    std::vector<TextureView> colorImageViews;
    std::vector<Texture> depthImages;
    std::vector<TextureView> depthImageViews;

    std::array<Gpu::Fence, MAX_FRAMES_IN_FLIGHT> frameCompletedFences;
    std::array<std::optional<Gpu::CommandBuffer>, MAX_FRAMES_IN_FLIGHT> commandBuffers;
    std::unique_ptr<KDGpuUtils::ResourceDeleter> resourceDeleter;
};

HeadlessFrameSource::HeadlessFrameSource(Context &context, HeadlessFrameSourceCreateInfo createInfo)
    : data_{std::make_unique<HeadlessFrameSourcePrivate>()}
{
    data_->ctx = &context;
    data_->label = std::move(createInfo.label);
    data_->extent = createInfo.size;
    data_->sampleCount =
        clampSampleCountToSupported(*context.device().adapter(), createInfo.samples);
    if (data_->sampleCount != createInfo.samples) {
        CO_CORE_WARN("Headless frame source '{}' requested sample count {}, clamped to {}",
                     data_->label,
                     magic_enum::enum_name(createInfo.samples),
                     magic_enum::enum_name(data_->sampleCount));
    }
    data_->colorFormat = createInfo.colorFormat;
    data_->depthFormat = selectDepthFormat(*context.device().adapter());
    data_->imageCount = std::max<size_t>(1, createInfo.imageCount);

    auto &device = context.device();

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        data_->frameCompletedFences[i] = device.createFence({
            .label = fmt::format("FENCE-{}-{}", data_->label, i),
            .createSignalled = true,
        });
    }

    const auto extent = data_->extent;
    data_->swapchainImages =
        ranges::views::indices(data_->imageCount) | ranges::views::transform([&](auto idx) {
            return device.createTexture(Gpu::TextureOptions{
                .label = fmt::format("TEX_HeadlessSwap[{}] {} (IMG)", idx, extent),
                .type = Gpu::TextureType::TextureType2D,
                .format = data_->colorFormat,
                .extent = {extent.x, extent.y, 1},
                .mipLevels = 1,
                .samples = Gpu::SampleCountFlagBits::Samples1Bit,
                .usage = Gpu::TextureUsageFlagBits::ColorAttachmentBit |
                         Gpu::TextureUsageFlagBits::TransferDstBit |
                         Gpu::TextureUsageFlagBits::TransferSrcBit |
                         Gpu::TextureUsageFlagBits::SampledBit,
                .memoryUsage = Gpu::MemoryUsage::GpuOnly,
                .createFlags = {},
            });
        }) |
        ranges::to<std::vector>;

    data_->swapchainViews =
        ranges::views::enumerate(data_->swapchainImages) |
        ranges::views::transform([extent](auto it) {
            auto [idx, image] = it;
            return image.createView(Gpu::TextureViewOptions{
                .label = fmt::format("TEX_HeadlessSwap[{}] {} (VIEW)", idx, extent),
            });
        }) |
        ranges::to<std::vector>;

    data_->colorImages =
        ranges::views::indices(MAX_FRAMES_IN_FLIGHT) | ranges::views::transform([&](auto idx) {
            return device.createTexture(Gpu::TextureOptions{
                .label = fmt::format("TEX_HeadlessColor[{}] {} (IMG)", idx, extent),
                .type = Gpu::TextureType::TextureType2D,
                .format = data_->colorFormat,
                .extent = {extent.x, extent.y, 1},
                .mipLevels = 1,
                .samples = data_->sampleCount,
                .usage = Gpu::TextureUsageFlagBits::ColorAttachmentBit |
                         Gpu::TextureUsageFlagBits::TransferSrcBit |
                         Gpu::TextureUsageFlagBits::SampledBit |
                         Gpu::TextureUsageFlagBits::StorageBit,
                .memoryUsage = Gpu::MemoryUsage::GpuOnly,
                .createFlags = {},
            });
        }) |
        ranges::to<std::vector>;

    data_->colorImageViews =
        ranges::views::enumerate(data_->colorImages) | ranges::views::transform([extent](auto it) {
            auto [idx, image] = it;
            return image.createView(Gpu::TextureViewOptions{
                .label = fmt::format("TEX_HeadlessColor[{}] {} (VIEW)", idx, extent),
            });
        }) |
        ranges::to<std::vector>;

    data_->depthImages =
        ranges::views::indices(MAX_FRAMES_IN_FLIGHT) | ranges::views::transform([&](auto idx) {
            return device.createTexture(Gpu::TextureOptions{
                .label = fmt::format("TEX_HeadlessDepth[{}] {} (IMG)", idx, extent),
                .type = Gpu::TextureType::TextureType2D,
                .format = data_->depthFormat,
                .extent = {extent.x, extent.y, 1},
                .mipLevels = 1,
                .samples = data_->sampleCount,
                .usage = Gpu::TextureUsageFlagBits::DepthStencilAttachmentBit |
                         Gpu::TextureUsageFlagBits::SampledBit,
                .memoryUsage = Gpu::MemoryUsage::GpuOnly,
                .createFlags = {},
            });
        }) |
        ranges::to<std::vector>;

    data_->depthImageViews =
        ranges::views::enumerate(data_->depthImages) | ranges::views::transform([extent](auto it) {
            auto [idx, image] = it;
            return image.createView(Gpu::TextureViewOptions{
                .label = fmt::format("TEX_HeadlessDepth[{}] {} (VIEW)", idx, extent),
            });
        }) |
        ranges::to<std::vector>;

    data_->resourceDeleter =
        std::make_unique<KDGpuUtils::ResourceDeleter>(&context.device(), MAX_FRAMES_IN_FLIGHT);
}

HeadlessFrameSource::~HeadlessFrameSource()
{
    CO_CORE_TRACE("Destroying Cory::HeadlessFrameSource.");
}

Gpu::Format HeadlessFrameSource::colorFormat() const noexcept
{
    return data_->colorFormat;
}

Gpu::Format HeadlessFrameSource::depthFormat() const noexcept
{
    return data_->depthFormat;
}

glm::u32vec2 HeadlessFrameSource::extent() const noexcept
{
    return data_->extent;
}

Gpu::SampleCountFlagBits HeadlessFrameSource::sampleCount() const noexcept
{
    return data_->sampleCount;
}

size_t HeadlessFrameSource::size() const noexcept
{
    return data_->swapchainImages.size();
}

cppcoro::generator<FrameContext> HeadlessFrameSource::frameGenerator()
{
    while (true) {
        const auto nextFrameIndex =
            static_cast<uint32_t>(data_->frameNumber % MAX_FRAMES_IN_FLIGHT);
        const auto swapchainImageIndex =
            static_cast<uint32_t>(data_->frameNumber % data_->swapchainImages.size());

        auto recorder = data_->ctx->device().createCommandRecorder(Gpu::CommandRecorderOptions{
            .label =
                fmt::format("CMD-Headless-Frame{:03}-[{}]", data_->frameNumber, nextFrameIndex),
            .queue = data_->ctx->graphicsQueue().handle(),
            .level = Gpu::CommandBufferLevel::Primary,
        });

        data_->frameCompletedFences[nextFrameIndex].wait();
        data_->frameCompletedFences[nextFrameIndex].reset();

        FrameContext frameCtx{
            .inFlightIndex = nextFrameIndex,
            .swapchainImageIndex = swapchainImageIndex,
            .frameNumber = data_->frameNumber,
            .extent = data_->extent,
            .colorFormat = data_->colorFormat,
            .depthFormat = data_->depthFormat,
            .sampleCount = data_->sampleCount,
            .swapchainImage = &data_->swapchainImages[swapchainImageIndex],
            .swapchainImageView = &data_->swapchainViews[swapchainImageIndex],
            .colorImage = &data_->colorImages[nextFrameIndex],
            .colorImageView = &data_->colorImageViews[nextFrameIndex],
            .depthImage = &data_->depthImages[nextFrameIndex],
            .depthImageView = &data_->depthImageViews[nextFrameIndex],
            .inFlight = &data_->frameCompletedFences[nextFrameIndex],
            .acquired = nullptr,
            .rendered = nullptr,
            .commandBuffer = std::move(recorder),
            .resourceDeleter = data_->resourceDeleter.get(),
        };

        ++data_->frameNumber;

        co_yield std::move(frameCtx);
    }
}

void HeadlessFrameSource::submit(FrameContext &frameCtx)
{
    const ScopeTimer s{"Headless/Submit"};

    auto commandBuffer = frameCtx.commandBuffer.finish();

    Gpu::SubmitOptions submitOptions{
        .commandBuffers = {commandBuffer},
        .waitSemaphores = {},
        .signalSemaphores = {},
        .signalFence = *frameCtx.inFlight,
    };
    data_->ctx->graphicsQueue().submit(submitOptions);

    data_->commandBuffers[frameCtx.inFlightIndex] = std::move(commandBuffer);
}

FrameGenerator HeadlessFrameSource::frames()
{
    return FrameGenerator{frameGenerator(), [this](FrameContext &frameCtx) { submit(frameCtx); }};
}

} // namespace Cory
