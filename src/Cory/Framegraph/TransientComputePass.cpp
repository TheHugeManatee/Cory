#include "TransientComputePass.hpp"

#include "ShaderBindingContext.hpp"

#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/DescriptorSets.hpp>
#include <Cory/Renderer/PipelineCache.hpp>
#include <Cory/Renderer/Shader.hpp>

#include <KDGpu/command_recorder.h>
#include <KDGpu/compute_pass_command_recorder.h>
#include <KDGpu/compute_pipeline_options.h>

#include <Cory/Base/Log.hpp>
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

TransientComputePass::~TransientComputePass()
{
    CO_CORE_ASSERT(currentRenderApi_ == nullptr || wasEnded_,
                   "TransientComputePass '{}' was not end()ed before destruction!",
                   pass_.name);
}

Gpu::ComputePassCommandRecorder TransientComputePass::begin(const RenderInput &renderApi)
{
    wasEnded_ = false;
    currentRenderApi_ = nullptr;
    bindingScope_.reset();

    auto options = Gpu::ComputePassCommandRecorderOptions{};
    auto recorder = renderApi.cmd->beginComputePass(std::move(options));

    // Set the pipeline layout so it is known for things like bind groups etc.
    recorder.setPipelineLayout(pipelineLayoutHandle());
    bindingScope_ = renderApi.bindingContext->scoped(recorder);
    currentRenderApi_ = &renderApi;

    return recorder;
}

void TransientComputePass::end(Gpu::ComputePassCommandRecorder &&recorder)
{
    CO_CORE_ASSERT(currentRenderApi_ != nullptr, "Begin was never called on this pass!");
    bindingScope_.reset();
    currentRenderApi_ = nullptr;
    recorder.end();

    wasEnded_ = true;
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

} // namespace Cory
