#pragma once

#include <Cory/Application/ApplicationLayer.hpp>
#include <Cory/Application/Common.hpp>
#include <Cory/Proper/Parameter.hpp>
#include <Cory/Proper/Property.hpp>

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

    NumericParameter<glm::vec3> position{"position",
                                         glm::vec3{0.0f, 0.0f, 5.0f},
                                         glm::vec3{-10.0f},
                                         glm::vec3{10.0f}};
    NumericParameter<glm::vec3> up{"up",
                                   glm::vec3{0.0f, 1.0f, 0.0f},
                                   glm::vec3{-1.0f},
                                   glm::vec3{1.0f}};
    NumericParameter<glm::vec3> focus{"focus",
                                      glm::vec3{0.0f, 0.0f, 0.0f},
                                      glm::vec3{-10.0f},
                                      glm::vec3{10.0f}};

    Parameter<float> roll{"roll", 0.0f};
    NumericParameter<float> fovy{"fovy", glm::radians(70.0f), glm::radians(10.0f), glm::radians(140.0f)};

    NumericParameter<float> rotationSpeed{"rotation speed", 0.002f, 0.001f, 0.1f};
    NumericParameter<float> movementSpeed{"movement speed", 0.010f, 0.01f, 1.0f};
    NumericParameter<float> scrollSpeed{"scroll speed", 0.25f, 0.1f, 10.0f};

    // external modifications to these will likely be ignored/overwritten
    Property<glm::vec3> forward{glm::vec3{0.0f, 0.0f, 1.0f}};   ///< =normalize(focus-position)
    Property<glm::vec3> right{glm::vec3{1.0f, 0.0f, 0.0f}};     ///< cross(forward, up)
    Property<glm::mat4x4> worldToViewMatrix{glm::mat4x4(1.0f)}; // the "View Matrix"
    Property<glm::mat4x4> viewToWorldMatrix{glm::mat4x4(1.0f)};

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
