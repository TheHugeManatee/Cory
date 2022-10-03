#include "TrianglePipeline.hpp"

#include <Cory/Base/Log.hpp>
#include <Cory/Core/Context.hpp>
#include <Cory/Core/ResourceLocator.hpp>

#include <Corrade/Containers/StringStlView.h>
#include <Magnum/Math/Range.h>
#include <Magnum/Vk/MeshLayout.h>
#include <Magnum/Vk/Pipeline.h>
#include <Magnum/Vk/PipelineLayoutCreateInfo.h>
#include <Magnum/Vk/RasterizationPipelineCreateInfo.h>
#include <Magnum/Vk/RenderPassCreateInfo.h>
#include <Magnum/Vk/ShaderSet.h>

namespace Vk = Magnum::Vk;

TrianglePipeline::TrianglePipeline(Cory::Context &context,
                                   glm::u32vec2 swapChainDimensions,
                                   std::filesystem::path vertFile,
                                   std::filesystem::path fragFile)
    : ctx_{context}
{
    createGraphicsPipeline(swapChainDimensions, std::move(vertFile), std::move(fragFile));
}

TrianglePipeline::~TrianglePipeline() = default;

void TrianglePipeline::createGraphicsPipeline(glm::u32vec2 swapChainDimensions,
                                              std::filesystem::path vertFile,
                                              std::filesystem::path fragFile)
{
    CO_APP_INFO("Starting shader compilation");
    vertexShader_ = Cory::Shader(ctx_, Cory::ShaderSource{Cory::ResourceLocator::locate(vertFile)});
    CO_APP_INFO("Vertex shader code size: {}", vertexShader_.size());
    fragmentShader_ =
        Cory::Shader(ctx_, Cory::ShaderSource{Cory::ResourceLocator::locate(fragFile)});
    CO_APP_INFO("Fragment shader code size: {}", fragmentShader_.size());

    Vk::ShaderSet shaderSet{};
    shaderSet.addShader(Vk::ShaderStage::Vertex, vertexShader_.module(), "main");
    shaderSet.addShader(Vk::ShaderStage::Fragment, fragmentShader_.module(), "main");

    Vk::MeshLayout meshLayout{Vk::MeshPrimitive::Triangles};
    // probably not needed but just to follow the tutorial exactly
    meshLayout.vkPipelineInputAssemblyStateCreateInfo().primitiveRestartEnable = false;

    Vk::PipelineLayout pipelineLayout{ctx_.device(), Vk::PipelineLayoutCreateInfo{}};

    Vk::RenderPass renderPass{
        ctx_.device(), Vk::RenderPassCreateInfo{}
        // todo
    };

    Vk::RasterizationPipelineCreateInfo rasterizationPipelineCreateInfo{
        shaderSet, meshLayout, pipelineLayout, renderPass, 0, 1};

    const glm::vec2 corner = swapChainDimensions;
    rasterizationPipelineCreateInfo.setViewport({{0.0f, 0.0f}, {corner.x, corner.y}});
    pipeline_ =
        std::make_unique<Vk::Pipeline>(ctx_.device(), std::move(rasterizationPipelineCreateInfo));
}
