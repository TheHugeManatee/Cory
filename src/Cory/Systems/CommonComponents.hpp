#pragma once

#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace Cory::Components {

enum class TransformMode {
    Local,
    World,
};

struct Transform {
    TransformMode mode{TransformMode::Local};

    glm::vec3 position{0.0f};

    /// Internal rotation representation in Y-X-Z (Tait-Bryan) convention.
    glm::quat orientation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};

    // updated by the system, other systems shouldn't modify this
    // TODO: split into two components?
    glm::mat4 modelToWorld{1.0f};
};

struct CameraComponent {
    glm::mat4 viewMatrix{1.0f};
    glm::vec3 position{0.0f};
    glm::vec3 direction{0.0f};
    float fovy{glm::radians(45.0f)};
    float nearPlane{5.0f};
    float farPlane{100.0f};
};

} // namespace Cory::Components
