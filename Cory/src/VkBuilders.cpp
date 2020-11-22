#include "VkBuilders.h"

#include "VkUtils.h"
#include "Context.h"

#include <fmt/format.h>
#include <ranges>

namespace Cory {
PipelineCreator &PipelineCreator::setShaders(std::vector<Shader> shaders)
{
    m_shaders = std::move(shaders);
    m_shaderCreateInfos.clear();
    std::ranges::transform(m_shaders, std::back_inserter(m_shaderCreateInfos),
                           [](auto &shader) { return shader.stageCreateInfo(); });

    // TODO: push constants here? Note: pSpecializationInfo can be used to set compile time
    // constants - kinda like macros in an online compilation?
    return *this;
}

PipelineCreator &PipelineCreator::setViewport(vk::Extent2D swapChainExtent)
{
    m_viewport = VkDefaults::Viewport(swapChainExtent);
    m_scissor = {{0, 0}, {swapChainExtent}};
    m_viewportState = VkDefaults::ViewportState(m_viewport, m_scissor);
    return *this;
}

PipelineCreator &PipelineCreator::setDefaultRasterizer()
{
    m_rasterizer = VkDefaults::Rasterizer();
    return *this;
}

PipelineCreator &PipelineCreator::setMultisampling(vk::SampleCountFlagBits samples)
{
    m_multisampling = VkDefaults::Multisampling(samples);
    return *this;
}

PipelineCreator &PipelineCreator::setDefaultDepthStencil()
{
    m_depthStencil = VkDefaults::DepthStencil();
    return *this;
}

PipelineCreator &PipelineCreator::setAttachmentBlendStates(
    std::vector<vk::PipelineColorBlendAttachmentState> blendStates)
{
    m_attachmentBlendStates = blendStates;
    m_colorBlending = VkDefaults::BlendState(&m_attachmentBlendStates);
    return *this;
}

PipelineCreator &PipelineCreator::setDefaultDynamicStates()
{
    m_dynamicStates = {vk::DynamicState::eViewport, vk::DynamicState::eLineWidth};

    m_dynamicState.dynamicStateCount = static_cast<uint32_t>(m_dynamicStates.size());
    m_dynamicState.pDynamicStates = m_dynamicStates.data();

    return *this;
}

PipelineCreator &PipelineCreator::setPipelineLayout(vk::PipelineLayout pipelineLayout)
{
    m_pipelineLayout = pipelineLayout;
    return *this;
}

PipelineCreator &PipelineCreator::setRenderPass(vk::RenderPass renderPass)
{
    m_renderPass = renderPass;
    return *this;
}

vk::UniquePipeline PipelineCreator::create(GraphicsContext &ctx)
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
} // namespace Cory