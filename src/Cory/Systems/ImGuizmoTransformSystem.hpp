#pragma once

#include <Cory/SceneGraph/System.hpp>
#include <Cory/Systems/CommonComponents.hpp>

#include <glm/mat4x4.hpp>

#include <unordered_map>

namespace Cory {

class ImGuizmoTransformSystem : public BasicSystem<ImGuizmoTransformSystem, Components::Transform> {
  public:
    using BasicSystem<ImGuizmoTransformSystem, Components::Transform>::BasicSystem;

    void beforeUpdate(SceneGraph &graph, uint64_t frameNumber);
    void
    update(SceneGraph &graph, TickInfo tickInfo, Entity entity, Components::Transform &transform);

  private:
    struct CameraData {
        glm::mat4 view{1.0f};
        glm::mat4 projection{1.0f};
        bool valid{false};
    };

    [[nodiscard]] glm::mat4 parentWorldMatrix(SceneGraph &graph, Entity entity) const;

    CameraData camera_;
    std::unordered_map<Entity, glm::mat4> worldMatrices_;
};

} // namespace Cory
