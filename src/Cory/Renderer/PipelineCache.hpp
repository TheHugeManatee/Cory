#pragma once

#include <Cory/Base/Math.hpp>
#include <Cory/Renderer/Common.hpp>
#include <Cory/Renderer/Gpu.hpp>

#include <KDGpu/graphics_pipeline_options.h>

#include <KDGpu/pipeline_layout_options.h>
#include <vector>

namespace Cory {

struct PipelineDescriptor {
    std::vector<ShaderHandle> shaders;
    Gpu::SampleCountFlagBits sampleCount;
    std::vector<Gpu::Format> colorFormats;
    Gpu::Format depthFormat;
    Gpu::Format stencilFormat;
    bool hasMeshInput;
    Gpu::PipelineLayoutHandle pipelineLayout;
    Gpu::VertexOptions vertexOptions;

    [[nodiscard]] std::size_t hash() const
    {
        return hashCompose(0,
                           shaders,
                           sampleCount,
                           colorFormats,
                           depthFormat,
                           stencilFormat,
                           pipelineLayout,
                           vertexOptions);
    }
    bool operator==(const PipelineDescriptor &rhs) const = default;
};

struct ComputePipelineDescriptor {
    ShaderHandle shader;
    Gpu::PipelineLayoutHandle pipelineLayout;

    [[nodiscard]] std::size_t hash() const { return hashCompose(0, shader, pipelineLayout); }
    bool operator==(const ComputePipelineDescriptor &rhs) const = default;
};

struct PipelineLayoutDescriptor {
    std::vector<Gpu::BindGroupLayoutHandle> bindGroupLayouts;
    std::vector<Gpu::PushConstantRange> pushConstantRanges;

    bool operator==(const PipelineLayoutDescriptor &rhs) const = default;
};

class PipelineCache {
  public:
    explicit PipelineCache(Gpu::VulkanResourceManager *resourceManager,
                           Gpu::DeviceHandle device,
                           ShaderManager *shaderManager);
    ~PipelineCache();

    Gpu::GraphicsPipelineHandle query(std::string_view name, const PipelineDescriptor &info);
    Gpu::ComputePipelineHandle queryComputePipeline(std::string_view name,
                                                    const ComputePipelineDescriptor &info);
    Gpu::PipelineLayoutHandle queryLayout(const Gpu::PipelineLayoutOptions &pipelineLayoutOptions);

  private:
    std::unique_ptr<struct PipelineCachePrivate> data_;
};

} // namespace Cory
