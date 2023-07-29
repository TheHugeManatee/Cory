#pragma once

#include <Cory/SceneGraph/System.hpp>

#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>

namespace Cory {

enum class TransformMode {
    Local,
    World,
};

struct Transform {
    TransformMode mode{TransformMode::Local};
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};
    // updated by the system, other systems shouldn't modify this
    // TODO: split into two components?
    glm::mat4 worldTransform{1.0f};
};

class TransformSystem : public BasicSystem<TransformSystem, Transform> {
  public:
    using BasicSystem<TransformSystem, Transform>::BasicSystem;
    void beforeUpdate(SceneGraph &graph);
    void update(SceneGraph &graph, TickInfo tickInfo, Entity entity, Transform &transform);
    void afterUpdate(SceneGraph &graph);
};
} // namespace Cory
