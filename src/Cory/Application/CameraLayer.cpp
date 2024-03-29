#include <Cory/Application/CameraLayer.hpp>

#include <Cory/Base/FmtUtils.hpp>
#include <Cory/Base/Utils.hpp>
#include <Cory/ImGui/Inputs.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/ResourceManager.hpp>

#include <glm/gtc/matrix_transform.hpp>

namespace Cory {

struct Uniforms {
    glm::vec2 center;
    glm::vec2 size;
    glm::vec2 window;
};
struct CameraLayer::State {
    // the current mode we're in based on the mouse button
    enum class Mode {
        None,  ///< no action
        Pan,   ///< pan in the plane of the screen
        Orbit, ///< orbit around the focus point
        Look,  ///< look up/down and left/right
        Roll   ///< Roll around the forward axis
    };

    glm::vec2 lastMousePosition{};
    glm::vec3 orbitUp{};
    glm::vec3 orbitRight{};

    Mode mode{};
};

CameraLayer::CameraLayer()
    : ApplicationLayer("Camera")
{
    lookAt(position(), focus(), up());
    // process internal and external changes in the update() function
    // position.valueChanged().connect([this]() { update(); });
    // focus.valueChanged().connect([this]() { update(); });
    // up.valueChanged().connect([this]() { update(); });
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

    update();
}

void CameraLayer::onDetach(Context &ctx)
{
    // might have had an exception during attach, or moved-from
    if (!state_) return;

    // auto &res = ctx.resources();

    state_.reset();
}

bool CameraLayer::onEvent(Event event)
{
    return std::visit(lambda_visitor{
                          [](auto event) { return false; },
                          [this](const ScrollEvent &event) { return mouseScroll(event); },
                          [this](const MouseMovedEvent &event) { return mouseMove(event); },
                          [this](const MouseButtonEvent &event) { return mouseButton(event); },
                      },
                      event);
}

void CameraLayer::onUpdate()
{
    update();
    if (::ImGui::Begin("CameraLayer")) {
        CoImGui::Text("Mode: {}", state_->mode);

        CoImGui::Slider("position", position, -10.0f, 10.0f);
        CoImGui::Slider("up", up, -1.0f, 1.0f);
        CoImGui::Slider("focus", focus, -10.0f, 10.0f);
        CoImGui::Slider("fovy", fovy, 10.0f, 140.0f);
        CoImGui::Slider("forward", forward, -1.0f, 1.0f);
        CoImGui::Slider("right", right, -1.0f, 1.0f);
        //
        CoImGui::Input("orbit_up", state_->orbitUp);
        CoImGui::Input("orbit_right", state_->orbitRight);
    }
    ::ImGui::End();
}

void CameraLayer::lookAt(glm::vec3 position, glm::vec3 focus, glm::vec3 up)
{
    viewToWorldMatrix = glm::lookAt(position, focus, up);
}

void CameraLayer::update()
{
    worldToViewMatrix = glm::inverse(viewToWorldMatrix());
    position = viewToWorldMatrix() * glm::vec4{0.0f, 0.0f, 0.0f, 1.0f};
    up = viewToWorldMatrix() * glm::vec4{localUp, 0.0f};
    forward = viewToWorldMatrix() * glm::vec4{localForward, 0.0f};
    right = viewToWorldMatrix() * glm::vec4{localRight, 0.0f};
}

bool CameraLayer::mouseButton(const MouseButtonEvent &event)
{
    if (event.action == ButtonAction::Press) {
        state_->lastMousePosition = event.position;
        state_->orbitUp = up();
        state_->orbitRight = right();
        return true;
    }

    return false;
}

bool CameraLayer::mouseMove(const MouseMovedEvent &event)
{
    using Mode = State::Mode;

    Mode &mode = state_->mode;
    mode = Mode::None;
    if (event.button == MouseButton::Left)
        mode = event.modifiers.is_set(ModifierFlagBits::Shift) ? Mode::Look : Mode::Orbit;
    else if (event.button == MouseButton::Right) {
        mode = Mode::Pan;
    }
    //    if (event.modifiers.is_set(ModifierFlagBits::Shift))
    //        mode = Mode::Pan;
    //    else if (event.modifiers.is_set(ModifierFlagBits::Alt))
    //        mode = Mode::Orbit;
    //    else if (event.modifiers.is_set(ModifierFlagBits::Ctrl))
    //        mode = Mode::Look;

    glm::vec2 delta = event.position - state_->lastMousePosition;
    state_->lastMousePosition = event.position;

    switch (mode) {
    case Mode::None:
        return false;
    case Mode::Look: {
        // Calculate the rotation around the up vector (azimuth) and the right vector (elevation)
        const float azimuth = delta.x * rotationSpeed();
        const float elevation = -delta.y * rotationSpeed();

        // Create a rotation matrix for the azimuth and elevation
        glm::mat4 newViewToWorld = rotate(viewToWorldMatrix(), azimuth, localUp);
        newViewToWorld = rotate(newViewToWorld, elevation, localRight);
        viewToWorldMatrix = newViewToWorld;
        break;
    }
    case Mode::Pan: {
        // "Pan" moves the camera in the plane of the screen
        const glm::vec3 panDelta = -localRight * delta.x - delta.y * localUp;
        viewToWorldMatrix = glm::translate(viewToWorldMatrix(), panDelta * movementSpeed());
        break;
    }
    case Mode::Orbit: {
        // Calculate the rotation around the up vector (azimuth) and the right vector (elevation)
        const float azimuth = delta.x * rotationSpeed();
        const float elevation = -delta.y * rotationSpeed();

        // Create a rotation matrix for the azimuth and elevation
        const glm::vec3 localFocus = worldToViewMatrix() * glm::vec4{focus(), 1.0f};
        auto newViewToWorld = viewToWorldMatrix();
        newViewToWorld = glm::translate(newViewToWorld, localFocus);
        newViewToWorld = rotate(newViewToWorld, azimuth, localUp);
        newViewToWorld = rotate(newViewToWorld, elevation, localRight);
        newViewToWorld = glm::translate(newViewToWorld, -localFocus);
        viewToWorldMatrix = newViewToWorld;
        break;
    }
    case Mode::Roll: {
        // "Roll" rotates the camera round the view axis, effectively rotating the up fector by a
        // given angle/delta around the forward axis todo
        break;
    }
    }

    return true;
}

bool CameraLayer::mouseScroll(const ScrollEvent &event)
{
    if (event.modifiers.is_set(ModifierFlagBits::Shift)) {}
    else {
        glm::vec3 movementDelta = localForward * event.scrollDelta.y * scrollSpeed();
        viewToWorldMatrix = glm::translate(viewToWorldMatrix(), movementDelta);
    }
    return true;
}

} // namespace Cory
