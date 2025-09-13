#pragma once

#include <Cory/Base/Math.hpp>
#include <Cory/Renderer/Common.hpp>
#include <Cory/Renderer/Gpu.hpp>

#include <KDGpu/graphics_pipeline_options.h>

#include <vector>

namespace std {
template <> struct hash<Gpu::VertexAttribute> {
    std::size_t operator()(const Gpu::VertexAttribute &s) const noexcept
    {
        return Cory::hashCompose(0, s.location, s.binding, s.format, s.offset);
    }
};
template <> struct hash<Gpu::VertexBufferLayout> {
    std::size_t operator()(const Gpu::VertexBufferLayout &s) const noexcept
    {
        return Cory::hashCompose(0, s.binding, s.stride, s.inputRate);
    }
};
template <> struct hash<Gpu::VertexOptions> {
    std::size_t operator()(const Gpu::VertexOptions &s) const noexcept
    {
        return Cory::hashCompose(0, s.attributes, s.buffers);
    }
};
} // namespace std

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

class PipelineCache {
  public:
    explicit PipelineCache(Context &ctx);
    ~PipelineCache();

    Gpu::GraphicsPipelineHandle query(std::string_view name, const PipelineDescriptor &info);

  private:
    std::unique_ptr<struct PipelineCachePrivate> data_;
};

} // namespace Cory
