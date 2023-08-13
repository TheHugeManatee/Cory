#include <Cory/Application/CameraLayer.hpp>

#include <Cory/Base/FmtUtils.hpp>
#include <Cory/Base/ResourceLocator.hpp>
#include <Cory/Base/Utils.hpp>
#include <Cory/Framegraph/CommandList.hpp>
#include <Cory/Framegraph/RenderTaskBuilder.hpp>
#include <Cory/Framegraph/TextureManager.hpp>
#include <Cory/ImGui/Inputs.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/DescriptorSets.hpp>
#include <Cory/Renderer/ResourceManager.hpp>
#include <Cory/Renderer/UniformBufferObject.hpp>

#include <Magnum/Vk/CommandBuffer.h>
#include <Magnum/Vk/Device.h>
#include <glm/ext/matrix_transform.hpp>

namespace Cory {

struct Uniforms {
    glm::vec2 center;
    glm::vec2 size;
    glm::vec2 window;
};
struct CameraLayer::State {};

CameraLayer::CameraLayer()
    : ApplicationLayer("Camera")
{

    position.valueChanged().connect([this]() { updateViewMatrix(); });
    focus.valueChanged().connect([this]() { updateViewMatrix(); });
    up.valueChanged().connect([this]() { updateViewMatrix(); });
}

CameraLayer::~CameraLayer()
{
    CO_CORE_ASSERT(!state_, "CameraLayer was not detached before it was destroyed!");
}

void CameraLayer::onAttach(Context &ctx, LayerAttachInfo info)
{
    CO_CORE_ASSERT(state_ == nullptr, "Layer was already attached!");

    state_ = std::make_unique<State>(State{

    });
}

void CameraLayer::onDetach(Context &ctx)
{
    // might have had an exception during attach, or moved-from
    if (!state_) return;

    auto &res = ctx.resources();

    state_.reset();
}

bool CameraLayer::onEvent(Event event)
{

    return std::visit(lambda_visitor{
                          [](auto event) { return false; },
                          [this](const SwapchainResizedEvent &event) {
                              //                              state_->viewportDimensions =
                              //                              event.size;
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
                              position = position.get() + forward.get() * size_delta.y;
                              return true;
                          },
                          [this](const MouseMovedEvent &event) {
                              //                              center = event.position /
                              //                              state_->viewportDimensions; return
                              //                              true;
                              return false;
                          },
                      },
                      event);
}

void CameraLayer::onUpdate()
{
    if (::ImGui::Begin("CameraLayer")) {
        CoImGui::Slider("center", position, -50.0f, 50.0f);
        CoImGui::Slider("focus", focus, -50.0f, 50.0f);
        CoImGui::Slider("fovy", fovy, 10.0f, 140.0f);
        CoImGui::Slider("up", up, -1.0f, 1.0f);
        CoImGui::Slider("roll", roll, -360.0f, 360.0f);
    }
    ::ImGui::End();
}

void CameraLayer::updateViewMatrix()
{
    glm::mat4 view = glm::lookAt(position.get(), focus.get(), up.get());

    if (abs(roll.get()) > 0.01f) {
        view = glm::rotate(view, roll.get(), forward.get());
        // view = view * rot;
    }

    viewMatrix = view;
}

} // namespace Cory
