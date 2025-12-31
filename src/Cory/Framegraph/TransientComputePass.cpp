#include "TransientComputePass.hpp"

#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/DescriptorSets.hpp>
#include <Cory/Renderer/PipelineCache.hpp>
#include <Cory/Renderer/Shader.hpp>

#include <KDGpu/command_recorder.h>
#include <KDGpu/compute_pass_command_recorder.h>
#include <KDGpu/compute_pipeline_options.h>

#include <array>

namespace Cory {

TransientComputePass::TransientComputePass(Context &ctx,
                                           FramegraphResourceManager &textures,
                                           ComputePassDeclaration pass)
    : ctx_{&ctx}
    , textures_{&textures}
    , pass_{std::move(pass)}
{
}

TransientComputePass::~TransientComputePass() {}

Gpu::ComputePassCommandRecorder TransientComputePass::begin(CommandRecorder &cmd)
{
    auto options = Gpu::ComputePassCommandRecorderOptions{};
    auto recorder = cmd.beginComputePass(std::move(options));

    recorder.setPipeline(pipelineHandle());
    return recorder;
}

Gpu::PipelineLayoutHandle TransientComputePass::pipelineLayoutHandle() noexcept
{
    if (pipelineLayout_.isValid()) {
        return pipelineLayout_;
    }

    pipelineLayout_ = ctx_->pipelineCache().queryLayout(Gpu::PipelineLayoutOptions{
        .label = fmt::format("Pipeline Layout {}", pass_.name),
        .bindGroupLayouts = ctx_->descriptors().layouts(),
        .pushConstantRanges = {Shader::globalPushConstantRange},
    });

    return pipelineLayout_;
}

Gpu::ComputePipelineHandle TransientComputePass::pipelineHandle() noexcept
{
    if (pipeline_.isValid()) {
        return pipeline_;
    }
    pipeline_ =
        ctx_->pipelineCache().queryComputePipeline(fmt::format("Compute Pipeline {}", pass_.name),
                                                   ComputePipelineDescriptor{
                                                       .pipelineLayout = pipelineLayoutHandle(),
                                                   });
    return pipeline_;
}

} // namespace Cory
