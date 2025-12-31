
#include <Cory/Framegraph/TransientRenderPass.hpp>

#include <Cory/Application/DynamicGeometry.hpp>
#include <Cory/Base/Log.hpp>
#include <Cory/Framegraph/Common.hpp>
#include <Cory/Framegraph/FramegraphResourceManager.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/DescriptorSets.hpp>
#include <Cory/Renderer/PipelineCache.hpp>
#include <Cory/Renderer/Shader.hpp>
#include <Cory/Renderer/ShaderManager.hpp>

#include <KDGpu/gpu_core.h>
#include <KDGpu/vulkan/vulkan_resource_manager.h>

#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>

namespace Cory {

namespace {
Gpu::CullModeFlagBits toCullMode(CullMode mode)
{
    switch (mode) {
    case CullMode::None:
        return Gpu::CullModeFlagBits::None;
    case CullMode::Front:
        return Gpu::CullModeFlagBits::FrontBit;
    case CullMode::Back:
        return Gpu::CullModeFlagBits::BackBit;
    case CullMode::FrontAndBack:
        return Gpu::CullModeFlagBits::FrontAndBack;
    default:
        return Gpu::CullModeFlagBits::BackBit;
    }
}

Gpu::CompareOperation toCompareOp(DepthTest test)
{
    switch (test) {
    case DepthTest::Disabled:
        return Gpu::CompareOperation::Always;
    case DepthTest::Less:
        return Gpu::CompareOperation::Less;
    case DepthTest::Greater:
        return Gpu::CompareOperation::Greater;
    case DepthTest::LessOrEqual:
        return Gpu::CompareOperation::LessOrEqual;
    case DepthTest::GreaterOrEqual:
        return Gpu::CompareOperation::GreaterOrEqual;
    case DepthTest::Always:
        return Gpu::CompareOperation::Always;
    case DepthTest::Never:
        return Gpu::CompareOperation::Never;
    default:
        return Gpu::CompareOperation::Less;
    }
}
} // namespace

TransientRenderPass::TransientRenderPass(Context &ctx,
                                         FramegraphResourceManager &textures,
                                         RenderPassDeclaration pass)
    : ctx_{&ctx}
    , textures_{&textures}
    , pass_{std::move(pass)}
    , dynamicStates_{pass_.dynamicStates}
{
}

TransientRenderPass::~TransientRenderPass() {}

Gpu::RenderPassCommandRecorder TransientRenderPass::begin(CommandRecorder &cmd)
{
    // if a render area has not been set up explicitly, we determine it by checking the attachments
    if (dynamicStates_.renderArea.offset.x == 0 && dynamicStates_.renderArea.offset.y == 0 &&
        dynamicStates_.renderArea.extent.width == 0 &&
        dynamicStates_.renderArea.extent.height == 0) {

        dynamicStates_.renderArea = determineRenderArea();
    }
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

    Gpu::RenderPassCommandRecorderWithDynamicRenderingOptions renderPassOptions{
        .colorAttachments = std::move(resolvedAttachments),
        .depthStencilAttachment = depthStencilAttachment.value_or(Gpu::DepthStencilAttachment{}),
        .samples = determineSampleCount(),
        .viewCount = 1,
        .framebufferWidth = renderArea.extent.width,
        .framebufferHeight = renderArea.extent.height,
        .framebufferArrayLayers = fbArrayLayers,
    };

    auto renderPassRecorder = cmd.beginRenderPass(renderPassOptions);

    if (!pass_.options.is_set(PassOptionFlagBits::SkipPipelineBind)) {
        CO_CORE_ASSERT(
            !pass_.shaders.empty(), "Render pass '{}' has no shaders to bind", pass_.name);
        std::vector<Gpu::ShaderStageFlags> stages;
        std::vector<Gpu::Handle<Gpu::ShaderObject_t>> handles;
        stages.reserve(pass_.shaders.size());
        handles.reserve(pass_.shaders.size());

        auto &shaders = ctx_->shaders();
        for (auto shaderHandle : pass_.shaders) {
            auto &shader = shaders[shaderHandle];
            stages.emplace_back(shader.type());
            handles.emplace_back(shader.shaderHandle());
        }
        renderPassRecorder.bindShaders(stages, handles);

        renderPassRecorder.setPrimitiveTopology(Gpu::PrimitiveTopology::TriangleList);
        renderPassRecorder.setFrontFace(Gpu::FrontFace::CounterClockwise);
        renderPassRecorder.setPolygonMode(Gpu::PolygonMode::Fill);
        renderPassRecorder.setRasterizationSamples(determineSampleCount());
        renderPassRecorder.setCullMode(toCullMode(pass_.dynamicStates.cullMode));
        renderPassRecorder.setRasterizerDiscardEnabled(false);

        const bool depthTestEnabled = pass_.dynamicStates.depthTest != DepthTest::Disabled;
        renderPassRecorder.setDepthTestEnabled(depthTestEnabled);
        renderPassRecorder.setDepthWriteEnabled(pass_.dynamicStates.depthWrite ==
                                                DepthWrite::Enabled);
        renderPassRecorder.setDepthCompareOp(toCompareOp(pass_.dynamicStates.depthTest));
        renderPassRecorder.setDepthBiasEnabled(false);
        renderPassRecorder.setDepthBoundsTestEnabled(false);
        renderPassRecorder.setDepthClampEnabled(false);
        renderPassRecorder.setStencilTestEnabled(false);
        renderPassRecorder.setAlphaToCoverageEnabled(false);
        renderPassRecorder.setAlphaToOneEnabled(false);
        renderPassRecorder.setLogicOpEnabled(false);
        renderPassRecorder.setPrimitiveRestartEnabled(false);

        const std::vector<Gpu::SampleMask> sampleMasks(1, 0xffffffffu);
        renderPassRecorder.setSampleMask(determineSampleCount(), sampleMasks);

        if (!pass_.options.is_set(PassOptionFlagBits::DisableMeshInput)) {
            const auto defaultVertexOptions = Gpu::VertexOptions{
                .buffers = {Gpu::VertexBufferLayout{
                    .binding = 0,
                    .stride = sizeof(Mesh::Vertex),
                    .inputRate = Gpu::VertexRate::Vertex,
                }},
                .attributes = Mesh::vertexAttributes(),
            };
            const auto vertexOptions = pass_.vertexOptions.value_or(defaultVertexOptions);
            renderPassRecorder.setVertexInput(vertexOptions.buffers, vertexOptions.attributes);
        }

        Gpu::Rect2D scissorRect = dynamicStates_.renderArea;
        if (scissorRect.extent.width == 0 && scissorRect.extent.height == 0) {
            scissorRect = renderArea;
        }
        renderPassRecorder.setViewportWithCount({Gpu::Viewport{
            .x = static_cast<float>(scissorRect.offset.x),
            .y = static_cast<float>(scissorRect.offset.y),
            .width = static_cast<float>(scissorRect.extent.width),
            .height = static_cast<float>(scissorRect.extent.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        }});
        renderPassRecorder.setScissorWithCount({scissorRect});

        if (!pass_.attachments.empty()) {
            Gpu::ColorComponentFlags colorMask{};
            colorMask.setFlag(Gpu::ColorComponentFlagBits::RedBit, true);
            colorMask.setFlag(Gpu::ColorComponentFlagBits::GreenBit, true);
            colorMask.setFlag(Gpu::ColorComponentFlagBits::BlueBit, true);
            colorMask.setFlag(Gpu::ColorComponentFlagBits::AlphaBit, true);

            for (size_t i = 0; i < pass_.attachments.size(); ++i) {
                const auto blend = pass_.attachments[i].blend.value_or(Gpu::BlendOptions{});
                const Gpu::ColorBlendEquation blendEquation{
                    .srcColorBlendFactor = blend.color.srcFactor,
                    .dstColorBlendFactor = blend.color.dstFactor,
                    .colorBlendOp = blend.color.operation,
                    .srcAlphaBlendFactor = blend.alpha.srcFactor,
                    .dstAlphaBlendFactor = blend.alpha.dstFactor,
                    .alphaBlendOp = blend.alpha.operation,
                };
                renderPassRecorder.setColorBlendEnabled(static_cast<uint32_t>(i),
                                                        {blend.blendingEnabled});
                renderPassRecorder.setColorBlendEquations(static_cast<uint32_t>(i),
                                                          {blendEquation});
                renderPassRecorder.setColorWriteMasks(static_cast<uint32_t>(i), {colorMask});
            }
        }
    }

    return renderPassRecorder;
}

Gpu::PipelineLayoutHandle TransientRenderPass::pipelineLayoutHandle() noexcept
{
    if (pipelineLayout_.isValid()) {
        return pipelineLayout_;
    }

    pipelineLayout_ = ctx_->pipelineCache().queryLayout(Gpu::PipelineLayoutOptions{
        .label = fmt::format("Pipeline Layout {}", pass_.name),
        .bindGroupLayouts = ctx_->descriptors().layouts(),
        .pushConstantRanges = {Shader::globalPushConstantRange},
    });

    return pipelineLayout_;
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
