#include "CubePipeline.hpp"

#include <Cory/Application/Window.hpp>
#include <Cory/Base/Log.hpp>
#include <Cory/Base/Profiling.hpp>
#include <Cory/Base/ResourceLocator.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/ResourceManager.hpp>
#include <Cory/Renderer/Shader.hpp>

#include <Corrade/Containers/StringStlView.h>
#include <Magnum/Math/Range.h>
#include <Magnum/Vk/DescriptorPool.h>
#include <Magnum/Vk/DescriptorSet.h>
#include <Magnum/Vk/DescriptorSetLayoutCreateInfo.h>
#include <Magnum/Vk/DescriptorType.h>
#include <Magnum/Vk/Mesh.h>
#include <Magnum/Vk/Pipeline.h>
#include <Magnum/Vk/PipelineLayoutCreateInfo.h>
#include <Magnum/Vk/PixelFormat.h>
#include <Magnum/Vk/RasterizationPipelineCreateInfo.h>
#include <Magnum/Vk/RenderPassCreateInfo.h>
#include <Magnum/Vk/ShaderSet.h>

namespace Vk = Magnum::Vk;

CubePipeline::CubePipeline(Cory::Context &context,
                           const Cory::Window &window,
                           const Magnum::Vk::Mesh &mesh,
                           std::filesystem::path vertFile,
                           std::filesystem::path fragFile)
    : ctx_{context}
{
    createGraphicsPipeline(window, mesh, std::move(vertFile), std::move(fragFile));
}

CubePipeline::~CubePipeline() = default;

Magnum::Vk::Pipeline &CubePipeline::pipeline() { return ctx_.resources()[pipeline_]; }

void CubePipeline::createGraphicsPipeline(const Cory::Window &window,
                                          const Magnum::Vk::Mesh &mesh,
                                          std::filesystem::path vertFile,
                                          std::filesystem::path fragFile)
{
    Cory::ResourceManager &resources = ctx_.resources();

    // set up the shaders
    CO_APP_TRACE("Starting shader compilation");
    vertexShader_ = resources.createShader(Cory::ResourceLocator::locate(vertFile));
    CO_APP_TRACE("Vertex shader code size: {}", resources[vertexShader_].size());
    fragmentShader_ = ctx_.resources().createShader(Cory::ResourceLocator::locate(fragFile));
    CO_APP_TRACE("Fragment shader code size: {}", resources[fragmentShader_].size());

    Vk::ShaderSet shaderSet{};
    shaderSet.addShader(Vk::ShaderStage::Vertex, resources[vertexShader_].module(), "main");
    shaderSet.addShader(Vk::ShaderStage::Fragment, resources[fragmentShader_].module(), "main");

    // set up the descriptor set layouts
    descriptorLayout_ =
        std::make_unique<Vk::DescriptorSetLayout>(ctx_.device(),
                                                  Vk::DescriptorSetLayoutCreateInfo{
                                                      {{0, Vk::DescriptorType::UniformBuffer}},
                                                      //{{0, Vk::DescriptorType::UniformBuffer}},
                                                  });

    // set up push constants
    // use max guaranteed memory of 128 bytes, for all shaders
    VkPushConstantRange pushConstantRange{
        .stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_ALL, .offset = 0, .size = 128};

    // create pipeline layout
    Vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo{*descriptorLayout_};
    pipelineLayoutCreateInfo->pushConstantRangeCount = 1;
    pipelineLayoutCreateInfo->pPushConstantRanges = &pushConstantRange;
    layout_ = std::make_unique<Vk::PipelineLayout>(ctx_.device(), pipelineLayoutCreateInfo);

    Vk::RasterizationPipelineCreateInfo rasterizationPipelineCreateInfo{
        shaderSet, mesh.layout(), *layout_, VK_NULL_HANDLE, 0, 1};

    // configure dynamic state - one viewport and scissor configured but no dimensions specified
    rasterizationPipelineCreateInfo.setDynamicStates(Vk::DynamicRasterizationState::Viewport |
                                                     Vk::DynamicRasterizationState::Scissor |
                                                     Vk::DynamicRasterizationState::CullMode);
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

    VkPipelineDepthStencilStateCreateInfo depthStencilState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VkCompareOp::VK_COMPARE_OP_LESS,
        .minDepthBounds = 0.0f,
        .maxDepthBounds = 1.0f,
    };

    rasterizationPipelineCreateInfo->pMultisampleState = &multisampling;
    rasterizationPipelineCreateInfo->pDepthStencilState = &depthStencilState;

    // set up dynamic rendering with VK_KHR_dynamic_rendering
    auto colorFormat = static_cast<VkFormat>(window.colorFormat());
    auto depthFormat = static_cast<VkFormat>(window.depthFormat());
    int32_t sampleCount = window.sampleCount();
    const VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &colorFormat,
        .depthAttachmentFormat = depthFormat,
        .stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
    };
    rasterizationPipelineCreateInfo->pNext = &pipelineRenderingCreateInfo;

    pipeline_ = ctx_.resources().createPipeline(rasterizationPipelineCreateInfo);
}

Magnum::Vk::DescriptorSet CubePipeline::allocateDescriptorSet()
{
    return ctx_.descriptorPool().allocate(*descriptorLayout_);
}
