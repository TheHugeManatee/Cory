#pragma once

#include <Cory/Application/ApplicationLayer.hpp>
#include <Cory/Application/Common.hpp>

#include "kdbindings/property.h"

namespace Cory {

/**
 * Implements a camera controller that reacts to mouse events to move the camera around.
 * Note: The properties are currently effectively read-only as the internal viewToWorld matrix
 *       is used as the single source of truth. External modifications will be ignored.
 */
class CameraLayer : public ApplicationLayer {
  public:
    CameraLayer();

    ~CameraLayer() override;

    void onAttach(Context &ctx, LayerAttachInfo info) override;
    void onDetach(Context &ctx) override;
    bool onEvent(Event event) override;
    void onUpdate(const LogicUpdateContext &updateContext) override;

    kdb::Property<glm::vec3> position{glm::vec3{0.0f, 0.0f, 15.0f}};
    kdb::Property<glm::vec3> up{glm::vec3{0.0f, 1.0f, 0.0f}};
    kdb::Property<glm::vec3> focus{glm::vec3{0.0f, 0.0f, 0.0f}};

    kdb::Property<float> roll{0.0f};
    kdb::Property<float> fovy{glm::radians(70.0f)};

    kdb::Property<float> rotationSpeed{0.002f};
    kdb::Property<float> movementSpeed{0.010f};
    kdb::Property<float> scrollSpeed{0.25f};

    // external modifications to these will likely be ignored/overwritten
    kdb::Property<glm::vec3> forward{glm::vec3{0.0f, 0.0f, 1.0f}};   ///< =normalize(focus-position)
    kdb::Property<glm::vec3> right{glm::vec3{1.0f, 0.0f, 0.0f}};     ///< cross(forward, up)
    kdb::Property<glm::mat4x4> worldToViewMatrix{glm::mat4x4(1.0f)}; // the "View Matrix"
    kdb::Property<glm::mat4x4> viewToWorldMatrix{glm::mat4x4(1.0f)};

    static constexpr glm::vec3 localRight = {1.0f, 0.0f, 0.0f};
    static constexpr glm::vec3 localUp = {0.0f, 1.0f, 0.0f};
    static constexpr glm::vec3 localForward = {0.0f, 0.0f, -1.0f};

    void lookAt(glm::vec3 position, glm::vec3 focus, glm::vec3 up);

  private:
    struct State;
    std::unique_ptr<State> state_;

    void update();

    bool mouseButton(const MouseButtonEvent &event);
    bool mouseMove(const MouseMovedEvent &event);
    bool mouseScroll(const ScrollEvent &event);
};

} // namespace Cory
