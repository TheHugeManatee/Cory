
#include <Cory/Framegraph/TransientRenderPass.hpp>

#include <Cory/Base/Math.hpp>
#include <Cory/Framegraph/Common.hpp>
#include <Cory/Framegraph/TextureManager.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/PipelineCache.hpp>
#include <Cory/Renderer/ShaderManager.hpp>

#include <KDGpu/gpu_core.h>
#include <KDGpu/vulkan/vulkan_resource_manager.h>

#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>

#include <unordered_map>

namespace Cory {

TransientRenderPass::~TransientRenderPass()
{
    if (hasBegun_) {
        CO_APP_WARN("TransientRenderPass: It seems that begin() was called without end()!");
    }
}

Gpu::RenderPassCommandRecorder TransientRenderPass::begin(CommandRecorder &cmd)
{
    hasBegun_ = true;
    auto getColorFormat = [&](const auto &attachment) {
        return textures_->info(attachment.target).format;
    };

    // if a render area has not been set up explicitly, we determine it by checking the attachments
    if (dynamicStates_.renderArea.offset.x == 0 && dynamicStates_.renderArea.offset.y == 0 &&
        dynamicStates_.renderArea.extent.width == 0 &&
        dynamicStates_.renderArea.extent.height == 0) {

        dynamicStates_.renderArea = determineRenderArea();
    }

    // determine color formats for all attachments
    PipelineDescriptor descriptor{
        .shaders = pass_.shaders,
        .sampleCount = determineSampleCount(),
        .colorFormats =
            pass_.attachments | ranges::views::transform(getColorFormat) | ranges::to<std::vector>,
        .depthFormat =
            pass_.depthAttachment.transform(getColorFormat).value_or(Gpu::Format::UNDEFINED),
        .stencilFormat =
            pass_.stencilAttachment.transform(getColorFormat).value_or(Gpu::Format::UNDEFINED),
        .hasMeshInput = pass_.meshInput == MeshInput::Enabled};

    auto resolvedAttachments =
        pass_.attachments | ranges::views::transform([this](const ColorAttachment a) {
            const auto &state = textures_->state(a.target);
            auto previousLayout =
                static_cast<Gpu::TextureLayout>(Sync::GetVkImageLayout(state.lastAccess));

            return Gpu::ColorAttachment{.view = textures_->imageView(a.target),
                                        .resolveView = {},
                                        .loadOperation = a.load,
                                        .storeOperation = a.store,
                                        .clearValue = a.clearColor,
                                        .initialLayout = previousLayout,
                                        .layout = Gpu::TextureLayout::ColorAttachmentOptimal,
                                        .finalLayout = Gpu::TextureLayout::ColorAttachmentOptimal};
        }) |
        ranges::to<std::vector>;

    auto depthStencilAttachment = pass_.depthAttachment.transform([&](DepthStencilAttachment a) {
        return Gpu::DepthStencilAttachment{
            .view = textures_->imageView(a.target),
            .depthLoadOperation = a.load,
            .depthStoreOperation = a.store,
            .depthClearValue = a.clearDepthStencil.depthClearValue,
            .stencilLoadOperation = a.load,
            .stencilStoreOperation = a.store,
            .stencilClearValue = a.clearDepthStencil.stencilClearValue,
            .initialLayout = static_cast<Gpu::TextureLayout>(
                Sync::GetVkImageLayout(textures_->state(a.target).lastAccess)),
            .layout = Gpu::TextureLayout::DepthStencilAttachmentOptimal,
            .finalLayout = Gpu::TextureLayout::DepthStencilAttachmentOptimal};
    });

    auto renderArea = determineRenderArea();

    // If we have color attachments, we keep the layers at 0 - this will make the framebuffer
    // layers implicitly have as many layers as the first attachment. Otherwise, we set to 1
    // as we assume it is a depth-only pass
    const uint32_t fbArrayLayers = resolvedAttachments.empty() ? 1 : 0;

    Gpu::RenderPassCommandRecorderOptions renderPassOptions{
        .colorAttachments = std::move(resolvedAttachments),
        .depthStencilAttachment = depthStencilAttachment.value_or(Gpu::DepthStencilAttachment{}),
        .samples = determineSampleCount(),
        .viewCount = 1,
        .framebufferWidth = renderArea.extent.width,
        .framebufferHeight = renderArea.extent.height,
        .framebufferArrayLayers = fbArrayLayers,
    };

    auto renderPassRecorder = cmd.beginRenderPass(renderPassOptions);

    // TODO - figure out whether we want to actually set dynamic states via the render pass
    // declaration or not
    // cmd.setupDynamicStates(dynamicStates_);
    return renderPassRecorder;
}

Gpu::SampleCountFlagBits TransientRenderPass::determineSampleCount() const
{
    auto sampleCount = [this](auto attachment) {
        return textures_->info(attachment.target).sampleCount;
    };

    if (!pass_.attachments.empty()) {
        return sampleCount(pass_.attachments.front());
    }
    // sample count of one is returned if there is no attachment at all!
    return pass_.depthAttachment.transform(sampleCount)
        .value_or(KDGpu::SampleCountFlagBits::Samples1Bit);
}

Gpu::Rect2D TransientRenderPass::determineRenderArea() const
{
    Gpu::Rect2D rect{};
    auto extent = [this](auto attachment) {
        const auto s = textures_->info(attachment.target).size;
        return Gpu::Extent2D{s.x, s.y};
    };
    if (pass_.attachments.empty()) {
        rect.extent = pass_.depthAttachment.transform(extent).value_or(Gpu::Extent2D{
            0, 0}); // sample count of zero is returned if there is no attachment at all!
    }
    else {
        rect.extent = extent(pass_.attachments.front());
    }
    return rect;
}

} // namespace Cory
