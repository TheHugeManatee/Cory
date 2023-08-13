#pragma once

#include <Cory/Application/ApplicationLayer.hpp>
#include <Cory/Application/Common.hpp>

#include "kdbindings/property.h"

namespace Cory {

class CameraLayer : public ApplicationLayer {
  public:
    CameraLayer();

    ~CameraLayer() override;

    void onAttach(Context &ctx, LayerAttachInfo info) override;
    void onDetach(Context &ctx) override;
    bool onEvent(Event event) override;
    void onUpdate() override;

    // users should modify these
    kdb::Property<glm::vec3> position{glm::vec3{0.0f, 0.0f, -10.0f}};
    kdb::Property<glm::vec3> up{glm::vec3{0.0f, 1.0f, 0.0f}};
    kdb::Property<glm::vec3> focus{glm::vec3{0.0f, 0.0f, 0.0f}};
    kdb::Property<float> roll{0.0f};
    kdb::Property<float> fovy{glm::radians(50.0f)};

    // modifications to these will likely be ignored/overwritten
    kdb::Property<glm::vec3> forward{glm::vec3{0.0f, 0.0f, 1.0f}}; ///< =normalize(focus-position)
    kdb::Property<glm::vec3> right{glm::vec3{1.0f, 0.0f, 0.0f}}; ///< cross(forward, up)
    kdb::Property<glm::mat4x4> viewMatrix{glm::mat4x4(1.0f)};

  private:
    struct State;
    std::unique_ptr<State> state_;

    void updateViewMatrix();
};

} // namespace Cory