#include "PipelineCache.hpp"

#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/Gpu.hpp>
#include <Cory/Renderer/Shader.hpp>
#include <Cory/Renderer/ShaderManager.hpp>

#include <KDGpu/graphics_pipeline_options.h>
#include <KDGpu/vulkan/vulkan_resource_manager.h>

namespace std {
template <> struct hash<Gpu::PushConstantRange> {
    std::size_t operator()(const Gpu::PushConstantRange &s) const noexcept
    {
        return Cory::hashCompose(0, s.offset, s.size, s.shaderStages);
    }
};
} // namespace std

namespace Cory {

struct PipelineCachePrivate {
    Gpu::VulkanResourceManager *resourceManager;
    Gpu::DeviceHandle device;
    ShaderManager *shaderManager;

    using DescriptorHasher = decltype([](const PipelineDescriptor &d) { return d.hash(); });
    std::unordered_map<PipelineDescriptor, Gpu::GraphicsPipelineHandle, DescriptorHasher> cache;

    using LayoutOptionsHasher = decltype([](const PipelineLayoutDescriptor &d) {
        return hashCompose(0, d.pushConstantRanges, d.bindGroupLayouts);
    });
    std::unordered_map<PipelineLayoutDescriptor, Gpu::PipelineLayoutHandle, LayoutOptionsHasher>
        layoutCache;

    Gpu::GraphicsPipelineHandle create(std::string_view name, const PipelineDescriptor &info);

    Gpu::PipelineLayoutHandle createLayout(std::string_view label,
                                           const Gpu::PipelineLayoutOptions &info);
};

Gpu::GraphicsPipelineHandle PipelineCache::query(std::string_view name,
                                                 const PipelineDescriptor &info)
{
    if (auto it = data_->cache.find(info); it != data_->cache.end()) {
        return it->second;
    }
    auto handle = data_->create(name, info);
    data_->cache.insert({info, handle});
    return handle;
}

Gpu::PipelineLayoutHandle
PipelineCache::queryLayout(const Gpu::PipelineLayoutOptions &pipelineLayoutOptions)
{
    const auto key = PipelineLayoutDescriptor{pipelineLayoutOptions.bindGroupLayouts,
                                              pipelineLayoutOptions.pushConstantRanges};
    if (auto it = data_->layoutCache.find(key); it != data_->layoutCache.end()) {
        return it->second;
    }
    auto handle = data_->createLayout(pipelineLayoutOptions.label, pipelineLayoutOptions);
    data_->layoutCache.insert({key, handle});
    return handle;
}

PipelineCache::PipelineCache(Gpu::VulkanResourceManager *resourceManager,
                             Gpu::DeviceHandle device,
                             ShaderManager *shaderManager)
    : data_{std::make_unique<PipelineCachePrivate>()}
{
    data_->resourceManager = resourceManager;
    data_->device = device;
    data_->shaderManager = shaderManager;
}
PipelineCache::~PipelineCache()
{
    // release all pipelines
    for (const auto &[_, pipeline] : data_->cache) {
        data_->resourceManager->deleteGraphicsPipeline(pipeline);
    }
    // release all pipeline layouts
    for (const auto &[_, layout] : data_->layoutCache) {
        data_->resourceManager->deletePipelineLayout(layout);
    }
}

Gpu::GraphicsPipelineHandle PipelineCachePrivate::create(std::string_view name,
                                                         const PipelineDescriptor &info)
{
    CO_CORE_INFO("Creating new pipeline for '{}' ({:X})", name, info.hash());

    // 1) Shaders -> KDGpu::ShaderStage list
    std::vector<Gpu::ShaderStage> shaderStages;
    shaderStages.reserve(info.shaders.size());
    for (auto shaderHandle : info.shaders) {
        const auto &s = (*shaderManager)[shaderHandle];
        shaderStages.push_back(Gpu::ShaderStage{
            .shaderModule = s.module(),
            .stage = s.type(),
            .entryPoint = s.entryPoint(),
        });
    }

    std::vector<Gpu::RenderTargetOptions> rtos;
    rtos.reserve(info.colorFormats.size());
    for (auto fmt : info.colorFormats) {
        rtos.push_back(Gpu::RenderTargetOptions{.format = fmt});
    }

    // 8) Build pipeline options
    const Gpu::GraphicsPipelineOptions gpOpts = {
        .label = name,
        .shaderStages = std::move(shaderStages),
        .layout = info.pipelineLayout,
        .vertex = info.vertexOptions,
        .renderTargets = std::move(rtos),
        .depthStencil =
            Gpu::DepthStencilOptions{
                // VK_PIPELINE_RENDERING_CREATE_INFO::depthAttachmentFormat
                .format = info.depthFormat,
                .depthTestEnabled = true, // was set via dynamic state, but default true here
                .depthWritesEnabled = true,
                .depthCompareOperation = Gpu::CompareOperation::Less,
            },
        // ds.depthBoundsTestEnabled / ds.minDepthBounds / ds.maxDepthBounds if you actually use
        // bounds,
        .primitive = Gpu::PrimitiveOptions{},
        .multisample =
            Gpu::MultisampleOptions{
                .samples = info.sampleCount,
                // .sampleShadingEnabled = true; // currently not supported in KDGpu
            },
        .dynamicState =
            Gpu::DynamicStateOptions{
                // TODO: Need to actually extend KDGPU further to provide the necessary
                // functions on RenderPassCommandRecorder. Note: Viewport and Scissor are
                // automatically added by KDGpu..
                .enabledDynamicStates =
                    {/*Gpu::DynamicState::Viewport,
                     Gpu::DynamicState::Scissor,*/
                     Gpu::DynamicState::CullMode,
                     Gpu::DynamicState::DepthTestEnable,
                     Gpu::DynamicState::DepthWriteEnable,
                     Gpu::DynamicState::DepthCompareOp},
            }
        // If you target a predefined RenderPass instead of dynamic rendering:
        // .renderPass = myRenderPass, .subpassIndex = 0
    };

    return resourceManager->createGraphicsPipeline(device, gpOpts);
}

Gpu::PipelineLayoutHandle
PipelineCachePrivate::createLayout(std::string_view label,
                                   const Gpu::PipelineLayoutOptions &pipelineLayoutOptions)
{
    CO_CORE_INFO("Creating new pipeline layout for '{}'", label);

    return resourceManager->createPipelineLayout(device, pipelineLayoutOptions);
}

} // namespace Cory
