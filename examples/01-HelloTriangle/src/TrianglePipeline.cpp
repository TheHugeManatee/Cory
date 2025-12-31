#include "TrianglePipeline.hpp"

#include <Cory/Application/Window.hpp>
#include <Cory/Base/Log.hpp>
#include <Cory/Base/ResourceLocator.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/Shader.hpp>

#include <KDGpu/graphics_pipeline_options.h>

struct TrianglePipeline::PrivateData {
    Cory::Context *ctx;
    KDGpu::GraphicsPipeline pipeline;
    KDGpu::PipelineLayout layout;
    KDGpu::RenderPass mainRenderPass;
};

TrianglePipeline::TrianglePipeline(Cory::Context &context,
                                   const Cory::Window &window,
                                   const Mesh &mesh,
                                   std::filesystem::path vertFile,
                                   std::filesystem::path fragFile)
    : data_{std::make_unique<PrivateData>()}
{
    data_->ctx = &context;
    createGraphicsPipeline(window, mesh, std::move(vertFile), std::move(fragFile));
}

TrianglePipeline::~TrianglePipeline() {}
KDGpu::RenderPass &TrianglePipeline::mainRenderPass() { return data_->mainRenderPass; }
KDGpu::GraphicsPipeline &TrianglePipeline::pipeline() { return data_->pipeline; }
KDGpu::PipelineLayout &TrianglePipeline::layout() { return data_->layout; }

void TrianglePipeline::createGraphicsPipeline(const Cory::Window &window,
                                              const Mesh &mesh,
                                              std::filesystem::path vertFile,
                                              std::filesystem::path fragFile)
{
    auto &device = data_->ctx->device();

    CO_APP_TRACE("Starting shader compilation for {} and {}", vertFile.string(), fragFile.string());
    const auto vertexFile = Cory::ResourceLocator::Locate(vertFile);

    const auto vertexShaderSource =
        Cory::ShaderSource{vertexFile, Gpu::ShaderStageFlagBits::VertexBit};
    auto vertexShader =
        device.createShaderModule(
            Cory::Shader::CompileToSpv(vertexShaderSource, false).value().spirv);

    const auto fragmentFile = Cory::ResourceLocator::Locate(fragFile);
    const auto fragmentShaderSource =
        Cory::ShaderSource{fragmentFile, Gpu::ShaderStageFlagBits::FragmentBit};
    auto fragmentShader =
        device.createShaderModule(
            Cory::Shader::CompileToSpv(fragmentShaderSource, false).value().spirv);

    // Create a pipeline layout (array of bind group layouts)
    const KDGpu::PipelineLayoutOptions pipelineLayoutOptions = {
        .label = "Triangle",
        .bindGroupLayouts = {},
        .pushConstantRanges =
            {
                {.offset = 0,
                 .size = 128,
                 .shaderStages = KDGpu::ShaderStageFlagBits::VertexBit |
                                 KDGpu::ShaderStageFlagBits::FragmentBit},
            },
    };
    data_->layout = device.createPipelineLayout(pipelineLayoutOptions);

    // Create a pipeline
    const KDGpu::GraphicsPipelineOptions pipelineOptions = {
        .label = "Triangle",
        .shaderStages =
            {
                {.shaderModule = vertexShader, .stage = KDGpu::ShaderStageFlagBits::VertexBit},
                {.shaderModule = fragmentShader, .stage = KDGpu::ShaderStageFlagBits::FragmentBit},
            },
        .layout = data_->layout,
        .vertex =
            {
                .buffers = {{.binding = 0, .stride = sizeof(KDGpu::VertexRate::Vertex)}},
                .attributes = {{
                                   // Position
                                   .location = 0,
                                   .binding = 0,
                                   .format = KDGpu::Format::R32G32B32_SFLOAT,
                               },
                               {
                                   // Color
                                   .location = 1,
                                   .binding = 0,
                                   .format = KDGpu::Format::R32G32B32_SFLOAT,
                                   .offset = sizeof(glm::vec3),
                               }},
            },
        .renderTargets = {{.format = window.colorFormat()}},
        .depthStencil =
            {
                .format = window.depthFormat(),
                .depthWritesEnabled = true,
                .depthCompareOperation = KDGpu::CompareOperation::Less,
            },
        .primitive{
            .cullMode = KDGpu::CullModeFlagBits::None,
        },
        .multisample =
            {
                .samples = window.samples(),
                .alphaToCoverageEnabled = false,
            },
    };
    data_->pipeline = device.createGraphicsPipeline(pipelineOptions);
}
