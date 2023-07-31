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

    /// the rotation in euler angles (corresponds to a tayt-bryan rotation, see Cory::makeTransform())
    glm::vec3 rotation{0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};

    // updated by the system, other systems shouldn't modify this
    // TODO: split into two components?
    glm::mat4 modelToWorld{1.0f};
};
} // namespace Cory::Components