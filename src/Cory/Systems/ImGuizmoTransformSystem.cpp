#include <Cory/Systems/ImGuizmoTransformSystem.hpp>

#include <Cory/Base/Math.hpp>
#include <Cory/SceneGraph/SceneGraph.hpp>

#include <ImGuizmo.h>
#include <imgui.h>

#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <limits>

namespace Cory {

using Components::Transform;
using Components::TransformMode;

namespace {

glm::mat4 localMatrixFromTransform(const Transform &transform)
{
    return makeTransform(transform.position, transform.orientation, transform.scale);
}

} // namespace

void ImGuizmoTransformSystem::beforeUpdate(SceneGraph &graph, [[maybe_unused]] uint64_t frameNumber)
{
    camera_.valid = false;
    worldMatrices_.clear();
    graph.sortByDepth<Transform>();

    if (!enabled_) {
        return;
    }

    if (ImGui::GetCurrentContext() == nullptr) {
        return;
    }

    auto *viewport = ImGui::GetMainViewport();
    if (viewport == nullptr || viewport->Size.x <= 0.0f || viewport->Size.y <= 0.0f) {
        return;
    }

    forEach<Components::CameraComponent>(
        graph, [this, viewport](Entity, const Components::CameraComponent &camera) {
            if (camera_.valid) {
                return;
            }

            const float aspect = viewport->Size.x / viewport->Size.y;
            camera_.view = camera.viewMatrix;
            camera_.projection =
                makePerspective(camera.fovy, aspect, camera.nearPlane, camera.farPlane);
            // ImGuizmo expects OpenGL-style clip-space orientation. Flip Vulkan Y here.
            camera_.projection[1][1] *= -1.0f;
            camera_.valid = true;
        });

    if (!camera_.valid) {
        return;
    }

    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetRect(viewport->Pos.x, viewport->Pos.y, viewport->Size.x, viewport->Size.y);
}

glm::mat4 ImGuizmoTransformSystem::parentWorldMatrix(SceneGraph &graph, Entity entity) const
{
    for (Entity ancestor : graph.ancestors(entity)) {
        auto *ancestorTransform = graph.getComponent<Transform>(ancestor);
        if (ancestorTransform == nullptr) {
            continue;
        }

        if (auto it = worldMatrices_.find(ancestor); it != worldMatrices_.end()) {
            return it->second;
        }
        return ancestorTransform->modelToWorld;
    }

    return glm::mat4{1.0f};
}

void ImGuizmoTransformSystem::update(SceneGraph &graph,
                                     [[maybe_unused]] TickInfo tickInfo,
                                     Entity entity,
                                     Transform &transform)
{
    if (!enabled_) {
        return;
    }

    const auto localMatrix = localMatrixFromTransform(transform);
    const auto parentMatrix =
        transform.mode == TransformMode::Local ? parentWorldMatrix(graph, entity) : glm::mat4{1.0f};

    glm::mat4 worldMatrix =
        transform.mode == TransformMode::Local ? parentMatrix * localMatrix : localMatrix;

    if (camera_.valid) {
        ImGuizmo::PushID(static_cast<int>(entity));

        const auto mode =
            transform.mode == TransformMode::Local ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
        if (ImGuizmo::Manipulate(glm::value_ptr(camera_.view),
                                 glm::value_ptr(camera_.projection),
                                 ImGuizmo::UNIVERSAL,
                                 mode,
                                 glm::value_ptr(worldMatrix))) {
            glm::mat4 updatedLocal = worldMatrix;
            if (transform.mode == TransformMode::Local) {
                const float determinant = glm::determinant(parentMatrix);
                if (std::abs(determinant) > std::numeric_limits<float>::epsilon()) {
                    updatedLocal = glm::inverse(parentMatrix) * worldMatrix;
                }
            }

            if (auto decomposed = decomposeTransform(updatedLocal, transform.scale)) {
                transform.position = decomposed->translation;
                transform.scale = decomposed->scale;
                transform.orientation = decomposed->orientation;
            }
        }

        ImGuizmo::PopID();
    }

    worldMatrices_[entity] = worldMatrix;
}

} // namespace Cory
