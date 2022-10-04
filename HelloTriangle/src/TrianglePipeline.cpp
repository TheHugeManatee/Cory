#include "TrianglePipeline.hpp"

#include <Cory/Base/Log.hpp>
#include <Cory/Base/ResourceLocator.hpp>
#include <Cory/RenderCore/Context.hpp>
#include <Cory/Renderer/SwapChain.hpp>

#include <Corrade/Containers/StringStlView.h>
#include <Magnum/Math/Range.h>
#include <Magnum/Vk/Image.h> // for Vk::ImageLayout
#include <Magnum/Vk/MeshLayout.h>
#include <Magnum/Vk/Pipeline.h>
#include <Magnum/Vk/PipelineLayoutCreateInfo.h>
#include <Magnum/Vk/PixelFormat.h>
#include <Magnum/Vk/RasterizationPipelineCreateInfo.h>
#include <Magnum/Vk/RenderPassCreateInfo.h>
#include <Magnum/Vk/ShaderSet.h>

namespace Vk = Magnum::Vk;

TrianglePipeline::TrianglePipeline(Cory::Context &context,
                                   const Cory::SwapChain &swapChain,
                                   std::filesystem::path vertFile,
                                   std::filesystem::path fragFile)
    : ctx_{context}
{
    createGraphicsPipeline(swapChain, std::move(vertFile), std::move(fragFile));
}

TrianglePipeline::~TrianglePipeline() = default;

void TrianglePipeline::createGraphicsPipeline(const Cory::SwapChain &swapChain,
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

    Vk::PixelFormat colorFormat = swapChain.colorFormat();
    Vk::PixelFormat depthFormat = swapChain.depthFormat();

    int32_t sampleCount = 1; // change eventually for multisampling?

    mainRenderPass_ = std::make_unique<Vk::RenderPass>(
        ctx_.device(),
        Vk::RenderPassCreateInfo{}
            .setAttachments(
                // color
                {Vk::AttachmentDescription{
                     colorFormat,
                     {Vk::AttachmentLoadOperation::Clear, Vk::AttachmentLoadOperation::DontCare},
                     {Vk::AttachmentStoreOperation::Store, Vk::AttachmentStoreOperation::DontCare},
                     Vk::ImageLayout::Undefined,
                     Vk::ImageLayout{
                         VK_IMAGE_LAYOUT_PRESENT_SRC_KHR}, // this is not mapped yet by magnum
                     sampleCount},
                 // depth
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
            .setDependencies(
                {Vk::SubpassDependency{Vk::SubpassDependency::External,           // srcSubpass
                                       0,                                         // dstSubpass
                                       Vk::PipelineStage::ColorAttachmentOutput | // srcStages
                                           Vk::PipelineStage::EarlyFragmentTests, //
                                       Vk::PipelineStage::ColorAttachmentOutput | // dstStages
                                           Vk::PipelineStage::EarlyFragmentTests, //
                                       Vk::Access{},                              // srcAccess
                                       Vk::Access::ColorAttachmentWrite |         // dstAccess
                                           Vk::Access::DepthStencilAttachmentWrite}}));

    Vk::RasterizationPipelineCreateInfo rasterizationPipelineCreateInfo{
        shaderSet, meshLayout, pipelineLayout, *mainRenderPass_, 0, 1};

    const glm::vec2 corner = swapChain.extent();
    rasterizationPipelineCreateInfo.setViewport({{0.0f, 0.0f}, {corner.x, corner.y}});
    pipeline_ =
        std::make_unique<Vk::Pipeline>(ctx_.device(), std::move(rasterizationPipelineCreateInfo));
}
