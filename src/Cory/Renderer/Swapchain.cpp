#include "FrameContext.hpp"

#include <Cory/Renderer/Swapchain.hpp>

#include <Cory/Base/FmtUtils.hpp>
#include <Cory/Base/GlmUtils.hpp>
#include <Cory/Base/Log.hpp>
#include <Cory/Base/Profiling.hpp>
#include <Cory/Renderer/Context.hpp>

#include <KDGpu/swapchain_options.h>
#include <KDGpu/texture.h>
#include <KDGpu/texture_options.h>
#include <KDGpuUtils/resource_deleter.h>

#include <range/v3/range/conversion.hpp>
#include <range/v3/view/enumerate.hpp>
#include <range/v3/view/indices.hpp>
#include <range/v3/view/transform.hpp>

#include <utility>

namespace Cory {

struct SwapchainSetup {
    KDGpu::Format format{KDGpu::Format::B8G8R8A8_UNORM};
    KDGpu::CompositeAlphaFlagBits compositeAlpha{KDGpu::CompositeAlphaFlagBits::OpaqueBit};
    KDGpu::TextureUsageFlags usageFlags{KDGpu::TextureUsageFlagBits::ColorAttachmentBit};
    KDGpu::Format depthFormat;
    KDGpu::TextureUsageFlags depthImageUsage_;
    std::vector<KDGpu::SampleCountFlagBits> supportedSampleCounts;

    KDGpu::Extent2D extent;
    bool showSurfaceCapabilities{false};
    std::string capabilitiesString;
    KDGpu::PresentMode presentMode;

    static SwapchainSetup determineSwapchainSetup(const KDGpu::Device &device,
                                                  const KDGpu::Surface &surface);
};

struct SwapchainPrivate {

    SwapchainPrivate(Context &ctx_, const KDGpu::Surface &surface, SwapchainCreateInfo createInfo);

    void createColorAndDepthResources(KDGpu::SampleCountFlagBits samples);

    std::expected<FrameContext, SwapchainError> nextImage();
    void present(FrameContext &fc);

    Context *ctx{};

    std::string swapchainName;

    uint64_t frameNumber{0};

    SwapchainSetup swapchainSetup;
    KDGpu::Swapchain swapchain;
    std::vector<KDGpu::TextureView> swapchainViews;

    std::vector<KDGpu::Texture> colorImages;
    std::vector<KDGpu::TextureView> colorImageViews;
    std::vector<KDGpu::Texture> depthImages;
    std::vector<KDGpu::TextureView> depthImageViews;
    std::vector<KDGpu::GpuSemaphore> presentCompleteSemaphores;
    std::vector<KDGpu::GpuSemaphore> renderCompleteSemaphores;

    std::array<KDGpu::Fence, MAX_FRAMES_IN_FLIGHT> frameCompletedFences;
    // Command buffers for each frame in flight - stored here so we can keep them alive
    // until the commands have executed
    std::array<std::optional<KDGpu::CommandBuffer>, MAX_FRAMES_IN_FLIGHT> commandBuffers;
    uint32_t currentSwapchainImageIndex_{0};
    uint32_t inFlightIndex_{0};
    std::unique_ptr<KDGpuUtils::ResourceDeleter> resourceDeleter;
};

SwapchainSetup SwapchainSetup::determineSwapchainSetup(const KDGpu::Device &device,
                                                       const KDGpu::Surface &surface)
{
    auto &adapter = *device.adapter();
    SwapchainSetup swapchainSetup;

    using namespace KDGpu;
    const AdapterSwapchainProperties swapchainProperties =
        device.adapter()->swapchainProperties(surface);

    // Choose a presentation mode from the ones supported
    constexpr std::array<PresentMode, 4> preferredPresentModes = {
        PresentMode::Mailbox, PresentMode::FifoRelaxed, PresentMode::Fifo, PresentMode::Immediate};
    const auto &availableModes = swapchainProperties.presentModes;
    for (const auto &presentMode : preferredPresentModes) {
        const auto it = std::find(availableModes.begin(), availableModes.end(), presentMode);
        if (it != availableModes.end()) {
            swapchainSetup.presentMode = presentMode;
            break;
        }
    }

    // Try to ensure that the chosen format is supported
    swapchainSetup.format = [&]() {
        for (const auto &availableFormat : swapchainProperties.formats) {
            if (availableFormat.format == swapchainSetup.format &&
                availableFormat.colorSpace == ColorSpace::SRgbNonlinear) {
                return availableFormat.format;
            }
        }
        // Fallback to first listed available format
        return swapchainProperties.formats[0].format;
    }();

    // Choose a depth format from the ones supported
    constexpr std::array preferredDepthFormat = {
        Format::D24_UNORM_S8_UINT,
        Format::D16_UNORM_S8_UINT,
        Format::D32_SFLOAT_S8_UINT,
        Format::D16_UNORM,
        Format::D32_SFLOAT,
    };
    for (const auto &depthFormat : preferredDepthFormat) {
        const FormatProperties formatProperties = adapter.formatProperties(depthFormat);
        if (formatProperties.optimalTilingFeatures &
            FormatFeatureFlagBit::DepthStencilAttachmentBit) {
            swapchainSetup.depthFormat = depthFormat;
            break;
        }
    }

    // Try to ensure that the chosen alpha composite mode is supported
    swapchainSetup.compositeAlpha = [&]() {
        const auto supportedCompositeAlpha =
            swapchainProperties.capabilities.supportedCompositeAlpha;

        if (supportedCompositeAlpha.testFlag(swapchainSetup.compositeAlpha))
            return swapchainSetup.compositeAlpha;

        // Try to return a single, known, supported alpha bit
        constexpr std::array compositeAlphaBits = {CompositeAlphaFlagBits::OpaqueBit,
                                                   CompositeAlphaFlagBits::PreMultipliedBit,
                                                   CompositeAlphaFlagBits::PostMultipliedBit,
                                                   CompositeAlphaFlagBits::InheritBit};
        for (const auto alphaBit : compositeAlphaBits) {
            if (supportedCompositeAlpha.testFlag(alphaBit)) return alphaBit;
        }

        // If all else fails, do not change
        return swapchainSetup.compositeAlpha;
    }();

    swapchainSetup.capabilitiesString =
        surfaceCapabilitiesToString(device.adapter()->swapchainProperties(surface).capabilities);

    constexpr std::array availableSampleCounts{
        SampleCountFlagBits::Samples1Bit,
        SampleCountFlagBits::Samples2Bit,
        SampleCountFlagBits::Samples4Bit,
        SampleCountFlagBits::Samples8Bit,
        SampleCountFlagBits::Samples16Bit,
        SampleCountFlagBits::Samples32Bit,
        SampleCountFlagBits::Samples64Bit,
    };

    // get all of the supported sample counts for the hardware
    {
        auto supported = device.adapter()->properties().limits.framebufferColorSampleCounts.toInt();
        assert(supported);

        for (auto sample : availableSampleCounts) {
            if (static_cast<int>(sample) & supported)
                swapchainSetup.supportedSampleCounts.push_back(sample);
        }
    }

    return swapchainSetup;
}

SwapchainPrivate::SwapchainPrivate(Context &ctx_,
                                   const KDGpu::Surface &surface,
                                   SwapchainCreateInfo createInfo)
{
    ctx = &ctx_;
    auto &device = ctx->device();
    swapchainName = std::move(createInfo.label);
    swapchainSetup = SwapchainSetup::determineSwapchainSetup(device, surface);
    // since MAX_FRAMES_IN_FLIGHT does not change, the present and complete semaphores only need
    // to be created once here. Create the present complete and render complete semaphores
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        // presentCompleteSemaphores[i] = device.createGpuSemaphore();
        // renderCompleteSemaphores[i] = device.createGpuSemaphore();
        frameCompletedFences[i] = device.createFence({
            .label = fmt::format("FENCE-{}-{}", swapchainName, i),
            // have to create signalled, so that we don't have to treat the
            // first frames as special cases, but simply always wait on the fence
            .createSignalled = true,
        });
    }

    using namespace KDGpu;
    const AdapterSwapchainProperties swapchainProperties =
        device.adapter()->swapchainProperties(surface);
    const SurfaceCapabilities &surfaceCapabilities = swapchainProperties.capabilities;

    const auto minImageExtent = glmu::u32vec2::from(surfaceCapabilities.minImageExtent);
    const auto maxImageExtent = glmu::u32vec2::from(surfaceCapabilities.maxImageExtent);

    swapchainSetup.extent =
        glmu::to<Extent2D>(glm::clamp(createInfo.size, minImageExtent, maxImageExtent));

    // Create a swapchain of images that we will render to.
    const SwapchainOptions swapchainOptions = {
        .surface = surface,
        .format = swapchainSetup.format,
        .minImageCount = getSuitableImageCount(surfaceCapabilities),
        .imageExtent = swapchainSetup.extent,
        .imageUsageFlags = swapchainSetup.usageFlags,
        .compositeAlpha = swapchainSetup.compositeAlpha,
        .presentMode = swapchainSetup.presentMode,
        .oldSwapchain = swapchain,
    };

    swapchain = device.createSwapchain(swapchainOptions);

    const auto &swapchainTextures = swapchain.textures();
    const auto swapchainTextureCount = swapchainTextures.size();

    swapchainViews.clear();
    presentCompleteSemaphores.clear();
    renderCompleteSemaphores.clear();
    swapchainViews.reserve(swapchainTextureCount);
    presentCompleteSemaphores.reserve(swapchainTextureCount);
    renderCompleteSemaphores.reserve(swapchainTextureCount);

    // For each swapchain texture, create a view and a sync object (semaphore)
    for (uint32_t i = 0; i < swapchainTextureCount; ++i) {
        auto view = swapchainTextures[i].createView({.format = swapchainOptions.format});
        swapchainViews.push_back(std::move(view));
        presentCompleteSemaphores.push_back(device.createGpuSemaphore(
            {.label = fmt::format("PRESENT-COMPLETE-{}-{}", swapchainName, i)}));
        renderCompleteSemaphores.push_back(device.createGpuSemaphore(
            {.label = fmt::format("RENDER-COMPLETE-{}-{}", swapchainName, i)}));
    }

    createColorAndDepthResources(createInfo.samples);

    resourceDeleter =
        std::make_unique<KDGpuUtils::ResourceDeleter>(&ctx->device(), MAX_FRAMES_IN_FLIGHT);
}

void SwapchainPrivate::createColorAndDepthResources(KDGpu::SampleCountFlagBits samples)
{
    auto &device = ctx->device();

    const glm::u32vec2 extent = {swapchainSetup.extent.width, swapchainSetup.extent.height};

    // COLOR images (multisampled)
    colorImages = ranges::views::indices(swapchain.textures().size()) |
                  ranges::views::transform([&](auto idx) {
                      // Create a depth texture to use for depth-correct rendering

                      return device.createTexture(KDGpu::TextureOptions{
                          .label = fmt::format("TEX_WndColor[{}] {} (IMG)", idx, extent),
                          .type = KDGpu::TextureType::TextureType2D,
                          .format = swapchainSetup.format,
                          .extent = {swapchainSetup.extent.width, swapchainSetup.extent.height, 1},
                          .mipLevels = 1,
                          .samples = samples,
                          .usage = KDGpu::TextureUsageFlagBits::ColorAttachmentBit |
                                   KDGpu::TextureUsageFlagBits::TransferSrcBit |
                                   KDGpu::TextureUsageFlagBits::SampledBit,
                          .memoryUsage = KDGpu::MemoryUsage::GpuOnly,
                      });
                  }) |
                  ranges::to<std::vector>;

    colorImageViews = ranges::views::enumerate(colorImages) |
                      ranges::views::transform([extent](auto it) {
                          auto [idx, depthImage] = it;

                          return depthImage.createView(KDGpu::TextureViewOptions{
                              .label = fmt::format("TEX_WndCol[{}] {} (VIEW)", idx, extent),
                          });
                      }) |
                      ranges::to<std::vector>;

    // DEPTH images
    depthImages = ranges::views::indices(swapchain.textures().size()) |
                  ranges::views::transform([&](auto idx) {
                      // Create a depth texture to use for depth-correct rendering
                      return device.createTexture(KDGpu::TextureOptions{
                          .label = fmt::format("TEX_WndDepth[{}] {} (IMG)", idx, extent),
                          .type = KDGpu::TextureType::TextureType2D,
                          .format = swapchainSetup.depthFormat,
                          .extent = {swapchainSetup.extent.width, swapchainSetup.extent.height, 1},
                          .mipLevels = 1,
                          .samples = samples,
                          .usage = KDGpu::TextureUsageFlagBits::DepthStencilAttachmentBit |
                                   swapchainSetup.depthImageUsage_,
                          .memoryUsage = KDGpu::MemoryUsage::GpuOnly,
                      });
                  }) |
                  ranges::to<std::vector>;

    depthImageViews = ranges::views::enumerate(depthImages) |
                      ranges::views::transform([extent](auto it) {
                          auto [idx, depthImage] = it;

                          return depthImage.createView(KDGpu::TextureViewOptions{
                              .label = fmt::format("TEX_WndDepth[{}] {} (VIEW)", idx, extent),
                          });
                      }) |
                      ranges::to<std::vector>;
}

std::expected<FrameContext, SwapchainError> SwapchainPrivate::nextImage()
{
    auto nextFrameIndex = static_cast<uint32_t>(frameNumber % MAX_FRAMES_IN_FLIGHT);

    auto recorder = ctx->device().createCommandRecorder(KDGpu::CommandRecorderOptions{
        .label = fmt::format("CMD-{}-[{}]", frameNumber, nextFrameIndex),
        .queue = ctx->graphicsQueue().handle(),
        .level = KDGpu::CommandBufferLevel::Primary,
    });

    frameCompletedFences[nextFrameIndex].wait();
    frameCompletedFences[nextFrameIndex].reset();
    // Todo clear up resources here via resource deleter

    uint32_t swapchainImageIndex{};
    const KDGpu::AcquireImageResult result =
        swapchain.getNextImageIndex(swapchainImageIndex, presentCompleteSemaphores[nextFrameIndex]);

    if (result != KDGpu::PresentResult::Success) {
        switch (result) {
            using enum KDGpu::PresentResult;
        case OutOfDate:
            return std::unexpected(SwapchainError::OutOfDate);
        case DeviceLost:
        case SurfaceLost:
        case OutOfMemory:
            return std::unexpected(SwapchainError::Lost);
        default:
            std::unreachable();
        }
    }

    FrameContext frameCtx{
        .inFlightIndex = nextFrameIndex,
        .swapchainImageIndex = swapchainImageIndex,
        .frameNumber = frameNumber,
        .extent = glmu::u32vec2::from(swapchainSetup.extent),
        .swapchainImage = &swapchain.textures()[swapchainImageIndex],
        .swapchainImageView = &swapchainViews[swapchainImageIndex],
        .colorImage = &colorImages[nextFrameIndex],
        .colorImageView = &colorImageViews[nextFrameIndex],
        .depthImage = &depthImages[nextFrameIndex],
        .depthImageView = &depthImageViews[nextFrameIndex],
        .inFlight = &frameCompletedFences[nextFrameIndex],
        .acquired = &presentCompleteSemaphores[nextFrameIndex],
        .rendered = &renderCompleteSemaphores[swapchainImageIndex],
        .commandBuffer = std::move(recorder),
        .resourceDeleter = resourceDeleter.get(),
    };

    ++frameNumber;

    return frameCtx;
}

void SwapchainPrivate::present(FrameContext &frameCtx)
{
    {
        const ScopeTimer s{"Window/Submit"};

        auto command_buffer = frameCtx.commandBuffer.finish();

        KDGpu::SubmitOptions submitOptions{
            .commandBuffers = {command_buffer},
            .waitSemaphores = {*frameCtx.acquired},
            .signalSemaphores = {*frameCtx.rendered},
            .signalFence = *frameCtx.inFlight,
        };
        ctx->graphicsQueue().submit(submitOptions);

        commandBuffers[frameCtx.inFlightIndex] = std::move(command_buffer);
    }
    {
        const ScopeTimer s{"Window/Present"};

        KDGpu::PresentOptions presentOptions = {.waitSemaphores = {*frameCtx.rendered},
                                                .swapchainInfos = {{
                                                    .swapchain = swapchain,
                                                    .imageIndex = frameCtx.swapchainImageIndex,
                                                }}};

        ctx->graphicsQueue().present(presentOptions);
    }
}

Swapchain::Swapchain(Context &ctx, const KDGpu::Surface &surface, SwapchainCreateInfo createInfo)
    : data_{std::make_unique<SwapchainPrivate>(ctx, surface, createInfo)}
{
}
Swapchain::~Swapchain() { CO_CORE_TRACE("Destroying Cory::Swapchain."); }
KDGpu::Format Swapchain::colorFormat() const noexcept { return data_->swapchainSetup.format; }
KDGpu::Format Swapchain::depthFormat() const noexcept { return data_->swapchainSetup.depthFormat; }
glm::u32vec2 Swapchain::extent() const noexcept
{
    return glmu::u32vec2::from(data_->swapchainSetup.extent);
}
size_t Swapchain::size() const noexcept { return data_->swapchainViews.size(); }

std::expected<FrameContext, SwapchainError> Swapchain::nextImage() { return data_->nextImage(); }

void Swapchain::present(FrameContext &fc) { data_->present(fc); }

} // namespace Cory