#include <Cory/Application/DepthDebugLayer.hpp>

#include "GLFWUtils.hpp"

#include <Cory/Base/FmtUtils.hpp>
#include <Cory/Base/ResourceLocator.hpp>
#include <Cory/Base/Utils.hpp>
#include <Cory/Framegraph/FramegraphResourceManager.hpp>
#include <Cory/Framegraph/RenderTaskBuilder.hpp>
#include <Cory/Framegraph/ShaderBindingContext.hpp>
#include <Cory/ImGui/Inputs.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/FrameContext.hpp>
#include <Cory/Renderer/Shader.hpp>
#include <Cory/Renderer/ShaderManager.hpp>

#include <KDGpu/device.h>
#include <KDGpu/sampler.h>

namespace Cory {

struct DrawData {
    glm::vec2 center;
    glm::vec2 size;
    glm::vec2 window;
    TextureHeapIndex textureIndex;
};
struct DepthDebugLayer::State {
    ShaderHandle fullscreenTriShader;
    ShaderHandle depthDebugShader;
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
        .depthDebugShader{res.createShader(
            ShaderSource{ResourceLocator::Locate("shaders/DepthDebug.frag.slang")})},
        .sampler = ctx.device().createSampler(Gpu::SamplerOptions{
            .label = "DepthDebugLayer sampler",
            .magFilter = Gpu::FilterMode::Linear,
            .minFilter = Gpu::FilterMode::Linear}),
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
    auto eventHandler = lambda_visitor{
        [](auto event) { return false; },
        [this](const SwapchainResizedEvent &event) {
            if (!renderEnabled()) return false;
            state_->viewportDimensions = event.size;
            return false;
        },
        [this](const ScrollEvent &event) {
            if (!renderEnabled()) return false;
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
            if (!renderEnabled()) return false;
            center = event.position / state_->viewportDimensions;
            return true;
        },
        [this](const KeyEvent &event) {
            if (event.action == GLFW_PRESS && event.key == GLFW_KEY_D &&
                event.modifiers == GLFW_MOD_CONTROL) {
                renderEnabled = !renderEnabled();
                return true;
            }
            return false;
        },
    };
    return std::visit(eventHandler, event);
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
    auto depthDebugPass = builder.declareRenderPass(RenderPassDeclaration{
        .name = "PASS_DepthDebug",
        .options = PassOptionFlagBits::DisableMeshInput,
        .shaders = {state_->fullscreenTriShader, state_->depthDebugShader},
        .attachments = {{
            .target = previousLayer.color,
            .load = Gpu::AttachmentLoadOperation::Load,
            .store = Gpu::AttachmentStoreOperation::Store,
            .clearColor = {},
            .blend = std::nullopt,
        }},
        .dynamicStates = {.cullMode = CullMode::None,
                          .depthTest = DepthTest::Disabled,
                          .depthWrite = DepthWrite::Disabled},
    });
    const auto colorOut = depthDebugPass.colorOutputs().front();

    builder.read(previousLayer.depth,
                 Gpu::TextureUsageFlagBits::SampledBit,
                 Sync::AccessType::FragmentShaderReadSampledImageOrUniformTexelBuffer);

    /// ^^^^     DECLARATION      ^^^^
    RenderInput renderApi = co_await builder.finishDeclaration(
        LayerPassOutputs{.color = colorOut, .depth = previousLayer.depth});
    /// vvvv  RENDERING COMMANDS  vvvv

    FramegraphResourceManager &resources = *renderApi.resources;

    const auto depthLayout = static_cast<Gpu::TextureLayout>(
        Sync::GetVkImageLayout(resources.state(previousLayer.depth).lastAccess));

    auto recorder = depthDebugPass.begin(renderApi);

    const auto textureIndex = renderApi.bindingContext->bindTexture2D(
        previousLayer.depth, depthLayout, state_->sampler.handle());

    auto d = renderApi.bindingContext->alloc<DrawData>();
    d->center = center.get();
    d->size = size.get();
    d->window = window.get();
    d->textureIndex = textureIndex;
    renderApi.bindingContext->push(d.gpu);
    renderApi.bindingContext->flush();
    recorder.draw(Gpu::DrawCommand{.vertexCount = 3, .instanceCount = 1});


    depthDebugPass.end(std::move(recorder));
}

} // namespace Cory
