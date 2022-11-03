#include "TrianglePipeline.hpp"

#include <Cory/Application/Window.hpp>
#include <Cory/Base/Log.hpp>
#include <Cory/Base/ResourceLocator.hpp>
#include <Cory/RenderCore/Context.hpp>

#include <Corrade/Containers/StringStlView.h>
#include <Magnum/Math/Range.h>
#include <Magnum/Vk/Image.h> // for Vk::ImageLayout
#include <Magnum/Vk/Mesh.h>
#include <Magnum/Vk/MeshLayout.h>
#include <Magnum/Vk/Pipeline.h>
#include <Magnum/Vk/PipelineLayoutCreateInfo.h>
#include <Magnum/Vk/PixelFormat.h>
#include <Magnum/Vk/RasterizationPipelineCreateInfo.h>
#include <Magnum/Vk/RenderPassCreateInfo.h>
#include <Magnum/Vk/ShaderSet.h>

namespace Vk = Magnum::Vk;

TrianglePipeline::TrianglePipeline(Cory::Context &context,
                                   const Cory::Window &window,
                                   const Magnum::Vk::Mesh &mesh,
                                   std::filesystem::path vertFile,
                                   std::filesystem::path fragFile)
    : ctx_{context}
{
    createGraphicsPipeline(window, mesh, std::move(vertFile), std::move(fragFile));
}

TrianglePipeline::~TrianglePipeline() = default;

void TrianglePipeline::createGraphicsPipeline(const Cory::Window &window,
                                              const Magnum::Vk::Mesh &mesh,
                                              std::filesystem::path vertFile,
                                              std::filesystem::path fragFile)
{
    CO_APP_TRACE("Starting shader compilation");
    vertexShader_ = Cory::Shader(ctx_, Cory::ShaderSource{Cory::ResourceLocator::locate(vertFile)});
    CO_APP_TRACE("Vertex shader code size: {}", vertexShader_.size());
    fragmentShader_ =
        Cory::Shader(ctx_, Cory::ShaderSource{Cory::ResourceLocator::locate(fragFile)});
    CO_APP_TRACE("Fragment shader code size: {}", fragmentShader_.size());

    Vk::ShaderSet shaderSet{};
    shaderSet.addShader(Vk::ShaderStage::Vertex, vertexShader_.module(), "main");
    shaderSet.addShader(Vk::ShaderStage::Fragment, fragmentShader_.module(), "main");

    // use max guaranteed memory of 128 bytes, for all shaders
    VkPushConstantRange pushConstantRange{
        .stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_ALL, .offset = 0, .size = 128};
    Vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    pipelineLayoutCreateInfo->pushConstantRangeCount = 1;
    pipelineLayoutCreateInfo->pPushConstantRanges = &pushConstantRange;

    layout_ = std::make_unique<Vk::PipelineLayout>(ctx_.device(), pipelineLayoutCreateInfo);

    Vk::PixelFormat colorFormat = window.colorFormat();
    Vk::PixelFormat depthFormat = window.depthFormat();

    int32_t sampleCount = window.sampleCount();

    mainRenderPass_ = std::make_unique<Vk::RenderPass>(
        ctx_.device(),
        Vk::RenderPassCreateInfo{}
            .setAttachments(
                {// offscreen color
                 Vk::AttachmentDescription{
                     colorFormat,
                     {Vk::AttachmentLoadOperation::Clear, Vk::AttachmentLoadOperation::DontCare},
                     {Vk::AttachmentStoreOperation::Store, Vk::AttachmentStoreOperation::DontCare},
                     Vk::ImageLayout::Undefined,
                     Vk::ImageLayout::ColorAttachment,
                     sampleCount},
                 // offscreen depth
                 Vk::AttachmentDescription{
                     depthFormat,
                     {Vk::AttachmentLoadOperation::Clear, Vk::AttachmentLoadOperation::DontCare},
                     {Vk::AttachmentStoreOperation::DontCare,
                      Vk::AttachmentStoreOperation::DontCare},
                     Vk::ImageLayout::Undefined,
                     Vk::ImageLayout::DepthStencilAttachment,
                     sampleCount}})
            .addSubpass(Vk::SubpassDescription{}
                            .setColorAttachments(
                                {Vk::AttachmentReference{0, Vk::ImageLayout::ColorAttachment}})
                            .setDepthStencilAttachment({Vk::AttachmentReference{
                                1, Vk::ImageLayout::DepthStencilAttachment}}))
            .setDependencies({Vk::SubpassDependency{
                Vk::SubpassDependency::External,          // srcSubpass
                0,                                        // dstSubpass
                Vk::PipelineStage::ColorAttachmentOutput, // srcStages
                Vk::PipelineStage::ColorAttachmentOutput, // dstStages
                Vk::Access{},                             // srcAccess
                Vk::Access::ColorAttachmentWrite,         // dstAccess
            }}));

    Vk::RasterizationPipelineCreateInfo rasterizationPipelineCreateInfo{
        shaderSet, mesh.layout(), *layout_, *mainRenderPass_, 0, 1};

    // configure dynamic state - one viewport and scissor configured but no dimensions specified
    rasterizationPipelineCreateInfo.setDynamicStates(Vk::DynamicRasterizationState::Viewport |
                                                     Vk::DynamicRasterizationState::Scissor);
    VkPipelineViewportStateCreateInfo viewportState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };
    rasterizationPipelineCreateInfo->pViewportState = &viewportState;

    // multisampling setup
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = (VkSampleCountFlagBits)window.sampleCount();

    rasterizationPipelineCreateInfo->pMultisampleState = &multisampling;

    pipeline_ =
        std::make_unique<Vk::Pipeline>(ctx_.device(), std::move(rasterizationPipelineCreateInfo));
}
