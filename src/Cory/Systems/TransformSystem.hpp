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
    static constexpr auto in_place_delete = true;

    TransformMode mode{TransformMode::Local};
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};

    glm::mat4 worldTransform{1.0f};
};

class TransformSystem : public SimpleSystem<TransformSystem, Transform> {
  public:
    using SimpleSystem<TransformSystem, Transform>::SimpleSystem;
    void beforeUpdate(SceneGraph &graph);
    void update(SceneGraph &graph, TickInfo tickInfo, Entity entity, Transform &transform);
    void afterUpdate(SceneGraph &graph);
};
} // namespace Cory
