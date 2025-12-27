#include <Cory/Application/DepthDebugLayer.hpp>

#include <Cory/Base/FmtUtils.hpp>
#include <Cory/Base/ResourceLocator.hpp>
#include <Cory/Base/Utils.hpp>
#include <Cory/Framegraph/FramegraphResourceManager.hpp>
#include <Cory/Framegraph/RenderTaskBuilder.hpp>
#include <Cory/ImGui/Inputs.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/DescriptorSets.hpp>
#include <Cory/Renderer/FrameContext.hpp>
#include <Cory/Renderer/ShaderManager.hpp>
#include <Cory/Renderer/UniformBufferObject.hpp>

#include <KDGpu/device.h>
#include <KDGpu/sampler.h>

namespace Cory {

struct Uniforms {
    glm::vec2 center;
    glm::vec2 size;
    glm::vec2 window;
};
struct DepthDebugLayer::State {
    ShaderHandle fullscreenTriShader;
    ShaderHandle depthDebugShader;
    UniformBufferObject<Uniforms> ubo;
    Gpu::Sampler sampler;

    glm::vec2 viewportDimensions{1.0f};
};

DepthDebugLayer::DepthDebugLayer()
    : ApplicationLayer("DepthDebug")
{
}

DepthDebugLayer::~DepthDebugLayer()
{
    CO_CORE_ASSERT(!state_, "DepthDebugLayer was not detached before it was destroyed!");
}

void DepthDebugLayer::onAttach(Context &ctx, LayerAttachInfo info)
{
    CO_CORE_ASSERT(state_ == nullptr, "Layer was already attached!");

    auto &res = ctx.shaders();
    state_ = std::make_unique<State>(State{
        .fullscreenTriShader{
            res.createShader(ResourceLocator::Locate("shaders/FullscreenTriangle.vert.slang"))},
        .depthDebugShader{
            res.createShader(ResourceLocator::Locate("shaders/DepthDebug.frag.slang"))},
        .ubo{Cory::UniformBufferObject<Uniforms>(ctx, info.maxFramesInFlight)},
        .sampler = ctx.device().createSampler(Gpu::SamplerOptions{
            .magFilter = Gpu::FilterMode::Linear, .minFilter = Gpu::FilterMode::Linear}),
        .viewportDimensions = info.viewportDimensions,
    });
}

void DepthDebugLayer::onDetach(Context &ctx)
{
    // might have had an exception during attach, or moved-from
    if (!state_) return;

    auto &res = ctx.shaders();
    res.release(state_->fullscreenTriShader);
    res.release(state_->depthDebugShader);

    state_.reset();
}

bool DepthDebugLayer::onEvent(Event event)
{
    if (!renderEnabled.get()) {
        return false;
    }
    return std::visit(lambda_visitor{
                          [](auto event) { return false; },
                          [this](const SwapchainResizedEvent &event) {
                              state_->viewportDimensions = event.size;
                              return false;
                          },
                          [this](const ScrollEvent &event) {
                              glm::vec2 size_delta{};
                              if (event.modifiers.is_set(ModifierFlagBits::Shift)) {
                                  size_delta.x = event.scrollDelta.y;
                              }
                              else {
                                  size_delta.y = event.scrollDelta.y;
                              }
                              size = size.get() + size_delta * 0.03f;
                              return true;
                          },
                          [this](const MouseMovedEvent &event) {
                              center = event.position / state_->viewportDimensions;
                              return true;
                          },
                      },
                      event);
}

void DepthDebugLayer::onUpdate(const LogicUpdateContext &updateCtx)
{
    if (::ImGui::Begin("DepthDebugLayer")) {
        if (bool is_enabled = renderEnabled.get(); ::ImGui::Checkbox("Enabled", &is_enabled)) {
            renderEnabled = is_enabled;
        }
        CoImGui::Slider("center", center, 0.0f, 1.0f);
        CoImGui::Slider("size", size, 0.0f, 1.0f);
        CoImGui::Slider("window", window, 0.0f, 1.0f);
    }
    ::ImGui::End();
}

RenderTaskDeclaration<LayerPassOutputs> DepthDebugLayer::renderTask(RenderTaskBuilder builder,
                                                                    LayerPassOutputs previousLayer)
{
    auto [writtenColorHandle, colorInfo] =
        builder.readWrite(previousLayer.color, Sync::AccessType::ColorAttachmentReadWrite);
    (void)colorInfo;
    builder.read(previousLayer.depth,
                 Sync::AccessType::FragmentShaderReadSampledImageOrUniformTexelBuffer);

    auto cubePass = builder.declareRenderPass(RenderPassDeclaration{
        .name = "PASS_DepthDebug",
        .options = PassOptionFlagBits::DisableMeshInput,
        .shaders = {state_->fullscreenTriShader, state_->depthDebugShader},
        .attachments = {{
            .target = writtenColorHandle,
            .load = Gpu::AttachmentLoadOperation::Load,
            .store = Gpu::AttachmentStoreOperation::Store,
            .clearColor = {},
            .blend = std::nullopt,
        }},
    });

    /// ^^^^     DECLARATION      ^^^^
    co_yield LayerPassOutputs{.color = writtenColorHandle, .depth = previousLayer.depth};
    RenderInput renderApi = co_await builder.finishDeclaration();
    /// vvvv  RENDERING COMMANDS  vvvv

    FrameContext &frameCtx = *renderApi.frameCtx;

    // update the uniform buffer
    Uniforms &frameUniforms = state_->ubo[frameCtx.inFlightIndex];
    frameUniforms.size = size.get();
    frameUniforms.center = center.get();
    frameUniforms.window = window.get();
    state_->ubo.flush(frameCtx.inFlightIndex);

    FramegraphResourceManager &resources = *renderApi.resources;

    const auto depthLayout = static_cast<Gpu::TextureLayout>(
        Sync::GetVkImageLayout(resources.state(previousLayer.depth).lastAccess));
    std::array<Gpu::TextureLayout, 1> layouts{depthLayout};
    std::array textures{resources.imageView(previousLayer.depth)};
    std::array samplers{state_->sampler.handle()};

    auto &descriptorSets = *renderApi.descriptors;
    descriptorSets
        .write(DescriptorSets::SetType::Frame, frameCtx.inFlightIndex, layouts, textures, samplers)
        .write(DescriptorSets::SetType::Frame, frameCtx.inFlightIndex, state_->ubo)
        .flushWrites();

    auto recorder = cubePass.begin(*renderApi.cmd);
    recorder.setDepthTestEnabled(false);
    recorder.setDepthWriteEnabled(false);
    descriptorSets.bind(recorder, frameCtx.inFlightIndex);
    recorder.draw(Gpu::DrawCommand{.vertexCount = 3, .instanceCount = 1});

    recorder.end();
}

} // namespace Cory
