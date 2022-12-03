#include <Cory/Framegraph/TransientRenderPass.hpp>

#include <Cory/Framegraph/CommandList.hpp>
#include <Cory/Framegraph/Common.hpp>
#include <Cory/Framegraph/TextureManager.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/ResourceManager.hpp>
#include <Cory/Renderer/Shader.hpp>

#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>

#include <Corrade/Containers/ArrayView.h>
#include <Magnum/Vk/ImageView.h>
#include <Magnum/Vk/PipelineLayout.h>
#include <Magnum/Vk/RasterizationPipelineCreateInfo.h>
#include <Magnum/Vk/Shader.h>
#include <Magnum/Vk/ShaderCreateInfo.h>
#include <Magnum/Vk/ShaderSet.h>

#include <gsl/narrow>

#include <unordered_map>

#include "Cory/Base/Math.hpp"

namespace Vk = Magnum::Vk;

namespace Cory::Framegraph {

struct PipelineDescriptor {
    std::vector<ShaderHandle> shaders;
    int32_t sampleCount;
    std::vector<VkFormat> colorFormats;
    VkFormat depthFormat;
    VkFormat stencilFormat;
    std::size_t hash() const
    {
        return hashCompose(0, shaders, sampleCount, colorFormats, depthFormat, stencilFormat);
    }
    bool operator==(const PipelineDescriptor &rhs) const = default;
};

class PipelineCache {
  public:
    PipelineHandle query(Context &ctx, std::string_view name, PipelineDescriptor &info)
    {
        if (auto it = cache_.find(info); it != cache_.end()) { return it->second; }
        auto handle = create(ctx, name, info);
        cache_.insert({info, handle});
        return handle;
    }

  private:
    PipelineHandle create(Context &ctx, std::string_view name, PipelineDescriptor &info)
    {
        CO_CORE_INFO("Creating new pipeline for '{}' ({:X})", name, info.hash());
        // set up shaders
        auto &resources = ctx.resources();
        Vk::ShaderSet shaderSet{};
        for (auto shaderHandle : info.shaders) {
            auto &shader = resources[shaderHandle];
            shaderSet.addShader(
                static_cast<Vk::ShaderStage>(shader.type()), shader.module(), "main");
        }

        Vk::RasterizationPipelineCreateInfo pipelineCreateInfo{
            shaderSet, ctx.defaultMeshLayout(), ctx.defaultPipelineLayout(), VK_NULL_HANDLE, 0, 1};

        // configure dynamic states
        pipelineCreateInfo.setDynamicStates(Vk::DynamicRasterizationState::Viewport |
                                            Vk::DynamicRasterizationState::Scissor |
                                            Vk::DynamicRasterizationState::CullMode |
                                            Vk::DynamicRasterizationState::DepthTestEnable |
                                            Vk::DynamicRasterizationState::DepthWriteEnable |
                                            Vk::DynamicRasterizationState::DepthCompareOperation);

        const VkPipelineViewportStateCreateInfo viewportState{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .scissorCount = 1,
        };
        pipelineCreateInfo->pViewportState = &viewportState;

        // multisampling setup
        const VkPipelineMultisampleStateCreateInfo multisampling{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = (VkSampleCountFlagBits)info.sampleCount,
            .sampleShadingEnable = VK_FALSE,
        };

        // note: depth setup is ignored and actually overridden the dynamic states, only stencil and
        // depth bounds are relevant here
        const VkPipelineDepthStencilStateCreateInfo depthStencilState{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .depthTestEnable = VK_TRUE,
            .depthWriteEnable = VK_TRUE,
            .depthCompareOp = VkCompareOp::VK_COMPARE_OP_LESS,
            .minDepthBounds = 0.0f,
            .maxDepthBounds = 1.0f,
        };

        pipelineCreateInfo->pMultisampleState = &multisampling;
        pipelineCreateInfo->pDepthStencilState = &depthStencilState;

        // set up KHR_dynamic_rendering information
        const VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            .colorAttachmentCount = gsl::narrow<uint32_t>(info.colorFormats.size()),
            .pColorAttachmentFormats = info.colorFormats.data(),
            .depthAttachmentFormat = info.depthFormat,
            .stencilAttachmentFormat = info.stencilFormat,
        };
        pipelineCreateInfo->pNext = &pipelineRenderingCreateInfo;

        return ctx.resources().createPipeline(name, pipelineCreateInfo);
    }
    using DescriptorHasher = decltype([](const PipelineDescriptor &d) { return d.hash(); });
    std::unordered_map<PipelineDescriptor, PipelineHandle, DescriptorHasher> cache_;
};

TransientRenderPass::TransientRenderPass(Context &ctx,
                                         std::string_view name,
                                         TextureResourceManager &textures)
    : ctx_{&ctx}
    , name_{name}
    , textures_{&textures}
{
}

TransientRenderPass::~TransientRenderPass()
{
    if (hasBegun_) {
        CO_APP_WARN("TransientRenderPass: It seems that begin() was called without end()!");
    }
}

void TransientRenderPass::begin(CommandList &cmd)
{
    hasBegun_ = true;
    auto getColorFormat = [&](const std::pair<TextureHandle, AttachmentKind> &h) {
        return toVk(textures_->info(h.first).format);
    };

    // if a render area has not been set up explicitly, we determine it by checking the attachments
    if (dynamicStates_.renderArea.offset.x == 0 && dynamicStates_.renderArea.offset.y == 0 &&
        dynamicStates_.renderArea.extent.width == 0 &&
        dynamicStates_.renderArea.extent.height == 0) {

        dynamicStates_.renderArea = determineRenderArea();
    }

    // determine color formats for all attachments
    PipelineDescriptor descriptor{
        .shaders = shaders_,
        .sampleCount = determineSampleCount(),
        .colorFormats =
            colorAttachments_ | ranges::views::transform(getColorFormat) | ranges::to<std::vector>,
        .depthFormat = depthAttachment_.transform(getColorFormat).value_or(VK_FORMAT_UNDEFINED),
        .stencilFormat =
            stencilAttachment_.transform(getColorFormat).value_or(VK_FORMAT_UNDEFINED)};

    // todo need to move this out of here, statics SUCK
    static PipelineCache cache;

    auto pipelineHandle = cache.query(*ctx_, name_, descriptor);

    auto toAttachment = [&](const std::pair<TextureHandle, AttachmentKind> &p) {
        return makeAttachmentInfo(p.first, p.second);
    };

    {
        // create the VkRenderingAttachmentInfo structs
        auto colorAttachmentDescs =
            colorAttachments_ | ranges::views::transform(toAttachment) | ranges::to<std::vector>;
        auto depthAttachmentDesc = depthAttachment_.transform(toAttachment);
        auto stencilAttachmentDesc = stencilAttachment_.transform(toAttachment);

        // fill the begin rendering info
        const VkRenderingInfo beginRenderingInfo{
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .pNext = nullptr,
            .flags = {},
            .renderArea = dynamicStates_.renderArea,
            .layerCount = 1,
            .viewMask = {},
            .colorAttachmentCount = static_cast<uint32_t>(colorAttachmentDescs.size()),
            .pColorAttachments = colorAttachmentDescs.data(),
            .pDepthAttachment = depthAttachmentDesc ? &depthAttachmentDesc.value() : nullptr,
            .pStencilAttachment = stencilAttachmentDesc ? &stencilAttachmentDesc.value() : nullptr};

        cmd.beginRenderPass(pipelineHandle, &beginRenderingInfo);
    }

    cmd.setupDynamicStates(dynamicStates_);
}

void TransientRenderPass::end(CommandList &cmd)
{
    cmd.endPass();
    hasBegun_ = false;
}

VkRenderingAttachmentInfo TransientRenderPass::makeAttachmentInfo(TextureHandle handle,
                                                                  AttachmentKind attachmentKind)
{
    return VkRenderingAttachmentInfo{.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                                     .imageView = textures_->imageView(handle),
                                     .imageLayout =
                                         toVkImageLayout(textures_->state(handle).layout),
                                     .loadOp = attachmentKind.loadOp,
                                     .storeOp = attachmentKind.storeOp,
                                     .clearValue = attachmentKind.clearValue};
}

int32_t TransientRenderPass::determineSampleCount() const
{
    auto sampleCount = [this](auto pair) { return textures_->info(pair.first).sampleCount; };

    if (!colorAttachments_.empty()) { return sampleCount(colorAttachments_.front()); }

    return depthAttachment_.or_else([this]() { return stencilAttachment_; })
        .transform(sampleCount)
        .value_or(0); // sample count of zero is returned if there is no attachment at all!
}

VkRect2D TransientRenderPass::determineRenderArea()
{
    VkRect2D rect{};
    auto extent = [this](auto pair) {
        const auto s = textures_->info(pair.first).size;
        return VkExtent2D{s.x, s.y};
    };
    if (colorAttachments_.empty()) {
        rect.extent =
            depthAttachment_.or_else([this]() { return stencilAttachment_; })
                .transform(extent)
                .value_or(VkExtent2D{
                    0, 0}); // sample count of zero is returned if there is no attachment at all!
    }
    else {
        rect.extent = extent(colorAttachments_.front());
    }
    return rect;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TransientRenderPassBuilder::TransientRenderPassBuilder(Context &ctx,
                                                       std::string_view name,
                                                       TextureResourceManager &textures)
    : renderPass_{ctx, name, textures}
{
}

TransientRenderPassBuilder::~TransientRenderPassBuilder() = default;

TransientRenderPassBuilder &TransientRenderPassBuilder::shaders(std::vector<ShaderHandle> shaders)
{
    renderPass_.shaders_ = std::move(shaders);
    return *this;
}

TransientRenderPassBuilder &TransientRenderPassBuilder::attach(TransientTextureHandle handle,
                                                               VkAttachmentLoadOp loadOp,
                                                               VkAttachmentStoreOp storeOp,
                                                               VkClearColorValue clearValue)
{
    renderPass_.colorAttachments_.emplace_back(
        handle.texture, AttachmentKind{loadOp, storeOp, {.color = clearValue}});
    return *this;
}

TransientRenderPassBuilder &TransientRenderPassBuilder::attachDepth(TransientTextureHandle handle,
                                                                    VkAttachmentLoadOp loadOp,
                                                                    VkAttachmentStoreOp storeOp,
                                                                    float clearValue)
{

    renderPass_.depthAttachment_ = std::make_pair(
        handle.texture,
        AttachmentKind{loadOp, storeOp, {.depthStencil = {.depth = clearValue, .stencil = 0}}});
    return *this;
}

TransientRenderPassBuilder &TransientRenderPassBuilder::attachStencil(TransientTextureHandle handle,
                                                                      VkAttachmentLoadOp loadOp,
                                                                      VkAttachmentStoreOp storeOp,
                                                                      uint32_t clearValue)
{
    renderPass_.stencilAttachment_ = std::make_pair(
        handle.texture,
        AttachmentKind{loadOp, storeOp, {.depthStencil = {.depth = 1.0f, .stencil = clearValue}}});
    return *this;
}

TransientRenderPass TransientRenderPassBuilder::finish() { return std::move(renderPass_); }

} // namespace Cory::Framegraph