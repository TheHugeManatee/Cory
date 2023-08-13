#pragma once

#include <Cory/SceneGraph/System.hpp>
#include <Cory/Systems/CommonComponents.hpp>

namespace Cory {

class TransformSystem : public BasicSystem<TransformSystem, Components::Transform> {
  public:
    using BasicSystem<TransformSystem, Components::Transform>::BasicSystem;
    void beforeUpdate(SceneGraph &graph);
    void
    update(SceneGraph &graph, TickInfo tickInfo, Entity entity, Components::Transform &transform);
    void afterUpdate(SceneGraph &graph);
};
} // namespace Cory
