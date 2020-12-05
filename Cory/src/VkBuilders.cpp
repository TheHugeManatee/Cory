#include "VkBuilders.h"

#include "Context.h"
#include "Mesh.h"
#include "VkUtils.h"

#include <fmt/format.h>
#include <ranges>

namespace Cory {
PipelineBuilder &PipelineBuilder::setShaders(std::vector<Shader> shaders)
{
    m_shaders = std::move(shaders);
    m_shaderCreateInfos.clear();
    std::ranges::transform(m_shaders, std::back_inserter(m_shaderCreateInfos),
                           [](auto &shader) { return shader.stageCreateInfo(); });

    // TODO: push constants here? Note: pSpecializationInfo can be used to set compile time
    // constants - kinda like macros in an online compilation?
    return *this;
}

Cory::PipelineBuilder &PipelineBuilder::setVertexInput(const Mesh &mesh)
{
    return setVertexInput(mesh.bindingDescription(), mesh.attributeDescriptions(), mesh.topology());
}

Cory::PipelineBuilder &PipelineBuilder::setVertexInput(
    const vk::VertexInputBindingDescription &bindingDescriptor,
    const std::vector<vk::VertexInputAttributeDescription> &attributeDescriptors,
    vk::PrimitiveTopology topology /*= vk::PrimitiveTopology::eTriangleList*/)
{
    m_vertexBindingDescription = bindingDescriptor;
    m_vertexAttributeDescriptions = attributeDescriptors;

    m_vertexInputInfo.vertexBindingDescriptionCount = 1;
    m_vertexInputInfo.pVertexBindingDescriptions = &m_vertexBindingDescription;
    m_vertexInputInfo.vertexAttributeDescriptionCount =
        static_cast<uint32_t>(m_vertexAttributeDescriptions.size());
    m_vertexInputInfo.pVertexAttributeDescriptions = m_vertexAttributeDescriptions.data();

    m_inputAssembly.topology = topology;
    m_inputAssembly.primitiveRestartEnable =
        false; // allows to break primitive lists with 0xFFFF index
    return *this;
}

PipelineBuilder &PipelineBuilder::setViewport(vk::Extent2D swapChainExtent)
{
    m_viewport = VkDefaults::Viewport(swapChainExtent);
    m_scissor = {{0, 0}, {swapChainExtent}};
    m_viewportState = VkDefaults::ViewportState(m_viewport, m_scissor);
    return *this;
}

PipelineBuilder &PipelineBuilder::setDefaultRasterizer()
{
    m_rasterizer = VkDefaults::Rasterizer();
    return *this;
}

PipelineBuilder &PipelineBuilder::setMultisampling(vk::SampleCountFlagBits samples)
{
    m_multisampling = VkDefaults::Multisampling(samples);
    return *this;
}

PipelineBuilder &PipelineBuilder::setDefaultDepthStencil()
{
    m_depthStencil = VkDefaults::DepthStencil();
    return *this;
}

PipelineBuilder &PipelineBuilder::setAttachmentBlendStates(
    std::vector<vk::PipelineColorBlendAttachmentState> blendStates)
{
    m_attachmentBlendStates = blendStates;
    m_colorBlending = VkDefaults::BlendState(&m_attachmentBlendStates);
    return *this;
}

PipelineBuilder &PipelineBuilder::setDefaultDynamicStates()
{
    m_dynamicStates = {vk::DynamicState::eViewport, vk::DynamicState::eLineWidth};

    m_dynamicState.dynamicStateCount = static_cast<uint32_t>(m_dynamicStates.size());
    m_dynamicState.pDynamicStates = m_dynamicStates.data();

    return *this;
}

PipelineBuilder &PipelineBuilder::setPipelineLayout(vk::PipelineLayout pipelineLayout)
{
    m_pipelineLayout = pipelineLayout;
    return *this;
}

PipelineBuilder &PipelineBuilder::setRenderPass(vk::RenderPass renderPass)
{
    m_renderPass = renderPass;
    return *this;
}

vk::UniquePipeline PipelineBuilder::create(GraphicsContext &ctx)
{
    vk::GraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.stageCount = static_cast<uint32_t>(m_shaderCreateInfos.size());
    pipelineInfo.pStages = m_shaderCreateInfos.data();
    pipelineInfo.pVertexInputState = &m_vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &m_inputAssembly;
    pipelineInfo.pViewportState = &m_viewportState;
    pipelineInfo.pRasterizationState = &m_rasterizer;
    pipelineInfo.pMultisampleState = &m_multisampling;
    pipelineInfo.pDepthStencilState = &m_depthStencil;
    pipelineInfo.pColorBlendState = &m_colorBlending;
    pipelineInfo.pDynamicState = nullptr; // skipped for now --why tho?
    pipelineInfo.layout = m_pipelineLayout;
    pipelineInfo.renderPass = m_renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = nullptr; // note: vulkan can have "base" and "derived"
                                               // pipelines when they are similar
    pipelineInfo.basePipelineIndex = -1;

    auto [result, pipeline] =
        ctx.device->createGraphicsPipelineUnique(nullptr, pipelineInfo).asTuple();
    switch (result) {
    case vk::Result::eSuccess:
        return std::move(pipeline);
    case vk::Result::ePipelineCompileRequiredEXT:
        throw std::runtime_error(fmt::format("Could not create pipeline: {}", result));
        break;
    default:
        assert(false); // should never happen
        return {};
    }
}

vk::AttachmentReference RenderPassBuilder::addColorAttachment(vk::Format format,
                                                              vk::SampleCountFlagBits samples)
{
    vk::AttachmentDescription colorAttachment{};
    colorAttachment.format = format;
    colorAttachment.samples = samples;
    colorAttachment.loadOp = vk::AttachmentLoadOp::eClear; // care about color
    colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
    colorAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare; // don't care about stencil
    colorAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    colorAttachment.initialLayout = vk::ImageLayout::eUndefined;
    colorAttachment.finalLayout = vk::ImageLayout::eColorAttachmentOptimal;

    auto attachmentRef = addAttachment(colorAttachment, vk::ImageLayout::eColorAttachmentOptimal);
    m_colorAttachmentRefs.push_back(attachmentRef);
    return attachmentRef;
}

vk::AttachmentReference
RenderPassBuilder::addColorAttachment(vk::AttachmentDescription colorAttachment)
{
    auto attachmentRef = addAttachment(colorAttachment, vk::ImageLayout::eColorAttachmentOptimal);
    m_colorAttachmentRefs.push_back(attachmentRef);
    return attachmentRef;
}

vk::AttachmentReference RenderPassBuilder::addDepthAttachment(vk::Format format,
                                                              vk::SampleCountFlagBits samples)
{
    assert(!m_depthStencilAttachmentRef.has_value() && "DepthStencil attachment is already set!");

    vk::AttachmentDescription depthAttachment{};
    depthAttachment.format = format;
    depthAttachment.samples = samples;
    depthAttachment.loadOp = vk::AttachmentLoadOp::eClear;
    depthAttachment.storeOp = vk::AttachmentStoreOp::eDontCare;
    depthAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    depthAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    depthAttachment.initialLayout = vk::ImageLayout::eUndefined;
    depthAttachment.finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

    m_depthStencilAttachmentRef =
        addAttachment(depthAttachment, vk::ImageLayout::eDepthStencilAttachmentOptimal);
    return *m_depthStencilAttachmentRef;
}

vk::AttachmentReference RenderPassBuilder::addResolveAttachment(vk::Format format, vk::ImageLayout finalLayout)
{
    vk::AttachmentDescription colorAttachmentResolve{};
    colorAttachmentResolve.format = format;
    colorAttachmentResolve.samples = vk::SampleCountFlagBits::e1;
    colorAttachmentResolve.loadOp = vk::AttachmentLoadOp::eDontCare;
    colorAttachmentResolve.storeOp = vk::AttachmentStoreOp::eStore;
    colorAttachmentResolve.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    colorAttachmentResolve.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    colorAttachmentResolve.initialLayout = vk::ImageLayout::eUndefined;
    colorAttachmentResolve.finalLayout = finalLayout;

    auto attachmentRef =
        addAttachment(colorAttachmentResolve, vk::ImageLayout::eColorAttachmentOptimal);
    m_resolveAttachmentRefs.push_back(attachmentRef);
    return attachmentRef;
}

uint32_t RenderPassBuilder::addDefaultSubpass()
{
    vk::SubpassDescription subpass{};
    subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpass.colorAttachmentCount = static_cast<uint32_t>(m_colorAttachmentRefs.size());
    subpass.pColorAttachments = m_colorAttachmentRefs.data();
    if (m_depthStencilAttachmentRef)
        subpass.pDepthStencilAttachment = &*m_depthStencilAttachmentRef;
    else
        subpass.pDepthStencilAttachment = nullptr;
    subpass.pResolveAttachments = m_resolveAttachmentRefs.data();

    return addSubpass(subpass);
}

uint32_t RenderPassBuilder::addSubpass(vk::SubpassDescription subpassDesc)
{
    uint32_t subpassIdx = static_cast<uint32_t>(m_subpasses.size());
    m_subpasses.emplace_back(std::move(subpassDesc));
    return subpassIdx;
}

vk::RenderPass RenderPassBuilder::create(GraphicsContext &ctx)
{
    vk::RenderPassCreateInfo renderPassInfo{};
    renderPassInfo.attachmentCount = static_cast<uint32_t>(m_attachments.size());
    renderPassInfo.pAttachments = m_attachments.data();
    renderPassInfo.subpassCount = static_cast<uint32_t>(m_subpasses.size());
    renderPassInfo.pSubpasses = m_subpasses.data();
    renderPassInfo.dependencyCount = static_cast<uint32_t>(m_subpassDependencies.size());
    renderPassInfo.pDependencies = m_subpassDependencies.data();

    return ctx.device->createRenderPass(renderPassInfo);
}

vk::AttachmentReference RenderPassBuilder::addAttachment(vk::AttachmentDescription desc,
                                                         vk::ImageLayout layout)
{
    uint32_t attachmentID = static_cast<uint32_t>(m_attachments.size());
    m_attachments.push_back(desc);
    vk::AttachmentReference attachmentRef{};
    attachmentRef.attachment = attachmentID;
    attachmentRef.layout = layout;
    return attachmentRef;
}

} // namespace Cory