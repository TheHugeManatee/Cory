
#include "ComponentEditorSystem.hpp"

#include <Cory/Base/Math.hpp>
#include <Cory/ImGui/Inputs.hpp>
#include <Cory/ImGui/Widgets.hpp>
#include <Cory/SceneGraph/SceneGraph.hpp>
#include <Cory/Systems/CommonComponents.hpp>

#include <glm/common.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

#include <ImGuizmo.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace Cory {

namespace {
constexpr float kMinNearPlane = 0.001f;
constexpr float kMinFarOffset = 0.001f;
constexpr float kMinScale = 0.001f;
constexpr float kMaxCameraFovDegrees = 179.0f;

enum class GizmoOperation {
    Translate,
    Rotate,
    Scale,
    Universal,
};

ImGuizmo::OPERATION toImGuizmoOperation(GizmoOperation operation)
{
    switch (operation) {
    case GizmoOperation::Translate:
        return ImGuizmo::TRANSLATE;
    case GizmoOperation::Rotate:
        return ImGuizmo::ROTATE;
    case GizmoOperation::Scale:
        return ImGuizmo::SCALE;
    case GizmoOperation::Universal:
    default:
        return ImGuizmo::UNIVERSAL;
    }
}

struct GizmoCamera {
    glm::mat4 view{1.0f};
    glm::mat4 projection{1.0f};
    bool valid{false};
};

glm::mat4 localMatrixFromTransform(const Components::Transform &transform)
{
    return makeTransform(transform.position, transform.orientation, transform.scale);
}

glm::vec3 normalizeOrDefault(glm::vec3 value, glm::vec3 fallback = {0.0f, 0.0f, -1.0f})
{
    const auto lenSq = glm::dot(value, value);
    if (lenSq <= std::numeric_limits<float>::epsilon()) {
        return fallback;
    }
    return value / std::sqrt(lenSq);
}

GizmoCamera findGizmoCamera(SceneGraph &graph, const ImGuiViewport &viewport)
{
    GizmoCamera result{};
    for (auto entity : graph.depthFirstTraversal()) {
        const auto *camera = graph.getComponent<Components::CameraComponent>(entity);
        if (camera == nullptr) {
            continue;
        }

        const float aspect = viewport.Size.x / viewport.Size.y;
        result.view = camera->viewMatrix;
        result.projection =
            makePerspective(camera->fovy, aspect, camera->nearPlane, camera->farPlane);
        // ImGuizmo expects OpenGL clip-space orientation.
        result.projection[1][1] *= -1.0f;
        result.valid = true;
        return result;
    }
    return result;
}

glm::mat4 parentWorldMatrix(SceneGraph &graph, Entity entity)
{
    for (Entity ancestor : graph.ancestors(entity)) {
        const auto *ancestorTransform = graph.getComponent<Components::Transform>(ancestor);
        if (ancestorTransform != nullptr) {
            return ancestorTransform->modelToWorld;
        }
    }
    return glm::mat4{1.0f};
}

void drawMatrix(const glm::mat4 &matrix)
{
    for (int row = 0; row < 4; ++row) {
        CoImGui::Text("{:>10.4f} {:>10.4f} {:>10.4f} {:>10.4f}",
                      matrix[0][row],
                      matrix[1][row],
                      matrix[2][row],
                      matrix[3][row]);
    }
}

void drawTransformEditor(SceneGraph &graph, Entity entity)
{
    auto *transform = graph.getComponent<Components::Transform>(entity);
    if (transform == nullptr) {
        return;
    }

    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        CoImGui::ComboBox("Mode", transform->mode);

        ImGui::DragFloat3("Position", &transform->position.x, 0.05f);

        auto eulerDegrees = glm::degrees(glm::eulerAngles(glm::normalize(transform->orientation)));
        if (ImGui::DragFloat3("Rotation (deg)", &eulerDegrees.x, 0.5f, -180.0f, 180.0f, "%.1f")) {
            transform->orientation = glm::normalize(glm::quat(glm::radians(eulerDegrees)));
        }

        auto uniformScale =
            std::max(std::max(transform->scale.x, transform->scale.y), transform->scale.z);
        if (ImGui::DragFloat("Uniform Scale", &uniformScale, 0.01f, kMinScale, 1000.0f, "%.3f")) {
            transform->scale = glm::vec3{uniformScale};
        }

        if (ImGui::DragFloat3("Scale",
                              &transform->scale.x,
                              0.01f,
                              kMinScale,
                              1000.0f,
                              "%.3f",
                              ImGuiSliderFlags_Logarithmic)) {
            transform->scale = glm::max(transform->scale, glm::vec3{kMinScale});
        }

        if (ImGui::Button("Reset Transform")) {
            transform->position = glm::vec3{0.0f};
            transform->orientation = glm::quat{1.0f, 0.0f, 0.0f, 0.0f};
            transform->scale = glm::vec3{1.0f};
        }

        if (ImGui::TreeNode("Model To World (read only)")) {
            drawMatrix(transform->modelToWorld);
            ImGui::TreePop();
        }
    }
}

void drawCameraEditor(SceneGraph &graph, Entity entity)
{
    auto *camera = graph.getComponent<Components::CameraComponent>(entity);
    if (camera == nullptr) {
        return;
    }

    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat3("Position", &camera->position.x, 0.05f);

        auto direction = normalizeOrDefault(camera->direction);
        if (ImGui::DragFloat3("Direction", &direction.x, 0.01f, -1.0f, 1.0f, "%.3f")) {
            camera->direction = normalizeOrDefault(direction);
        }

        auto yawDegrees = glm::degrees(std::atan2(direction.z, direction.x));
        auto pitchDegrees = glm::degrees(std::asin(glm::clamp(direction.y, -1.0f, 1.0f)));
        const bool yawChanged =
            ImGui::DragFloat("Yaw (deg)", &yawDegrees, 0.25f, -180.0f, 180.0f, "%.2f");
        const bool pitchChanged =
            ImGui::DragFloat("Pitch (deg)", &pitchDegrees, 0.25f, -89.9f, 89.9f, "%.2f");
        if (yawChanged || pitchChanged) {
            const auto yaw = glm::radians(yawDegrees);
            const auto pitch = glm::radians(pitchDegrees);
            camera->direction = glm::normalize(glm::vec3{
                std::cos(pitch) * std::cos(yaw),
                std::sin(pitch),
                std::cos(pitch) * std::sin(yaw),
            });
        }

        auto fovDegrees = glm::degrees(camera->fovy);
        if (ImGui::SliderFloat("Vertical FOV (deg)", &fovDegrees, 5.0f, 120.0f, "%.1f")) {
            camera->fovy = glm::radians(glm::clamp(fovDegrees, 1.0f, kMaxCameraFovDegrees));
        }

        if (ImGui::DragFloat(
                "Near Plane", &camera->nearPlane, 0.01f, kMinNearPlane, 10000.0f, "%.3f")) {
            camera->nearPlane = std::max(camera->nearPlane, kMinNearPlane);
            camera->farPlane = std::max(camera->farPlane, camera->nearPlane + kMinFarOffset);
        }

        if (ImGui::DragFloat("Far Plane",
                             &camera->farPlane,
                             0.1f,
                             camera->nearPlane + kMinFarOffset,
                             100000.0f,
                             "%.3f")) {
            camera->farPlane = std::max(camera->farPlane, camera->nearPlane + kMinFarOffset);
        }

        if (ImGui::TreeNode("View Matrix (read only)")) {
            drawMatrix(camera->viewMatrix);
            ImGui::TreePop();
        }
    }
}

void drawPointLightEditor(SceneGraph &graph, Entity entity)
{
    auto *pointLight = graph.getComponent<Components::PointLightComponent>(entity);
    if (pointLight == nullptr) {
        return;
    }

    if (ImGui::CollapsingHeader("Point Light", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::ColorEdit3(
            "Color", &pointLight->color.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);

        if (ImGui::DragFloat("Intensity",
                             &pointLight->intensity,
                             0.05f,
                             0.0f,
                             100000.0f,
                             "%.3f",
                             ImGuiSliderFlags_Logarithmic)) {
            pointLight->intensity = std::max(pointLight->intensity, 0.0f);
        }

        const auto radiance = pointLight->color * pointLight->intensity;
        CoImGui::Text(
            "Effective radiance: [{:.2f}, {:.2f}, {:.2f}]", radiance.x, radiance.y, radiance.z);
    }
}
} // namespace

struct ComponentEditorSystem::ComponentEditorPrivate {
    struct EditorEntry {
        std::string name;
        EditorFunction callback;
    };
    std::vector<EditorEntry> editorFunctions;
    Entity selectedEntity{0};
    bool gizmoEnabled{true};
    GizmoOperation gizmoOperation{GizmoOperation::Universal};
    bool gizmoSnappingEnabled{false};
    glm::vec3 translateSnap{0.1f, 0.1f, 0.1f};
    float rotateSnapDegrees{5.0f};
    float scaleSnap{0.1f};

    void drawEntity(SceneGraph &graph, Entity entity)
    {
        ImGui::PushID(static_cast<int>(entity));
        const EntityMetaData &info = graph.data(entity);

        // Flags:
        // - OpenOnArrow: only arrow opens/closes (clicking label selects)
        // - SpanAvailWidth: full-row hitbox
        // - Selected: highlight current selection
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth |
                                   (info.children.empty() ? ImGuiTreeNodeFlags_Leaf : 0) |
                                   (selectedEntity == entity ? ImGuiTreeNodeFlags_Selected : 0);

        // TreeNodeEx returns true if opened and needs child rendering.
        const bool open = ImGui::TreeNodeEx("##node", flags, "%s", info.name.c_str());

        // Selection on click (on the item created by TreeNodeEx)
        if (ImGui::IsItemClicked()) {
            selectedEntity = entity;
        }

        if (open) {
            if (info.children.size() > 256) {
                ImGuiListClipper clipper;
                clipper.Begin(static_cast<int>(info.children.size()));
                while (clipper.Step()) {
                    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                        drawEntity(graph, info.children[static_cast<size_t>(i)]);
                    }
                }
            }
            else {
                for (auto child : info.children) {
                    drawEntity(graph, child);
                }
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    void drawSelectedComponents(SceneGraph &graph)
    {
        if (selectedEntity == 0) {
            CoImGui::Text("No entity selected");
            return;
        }

        auto *selectedTransform = graph.getComponent<Components::Transform>(selectedEntity);
        if (ImGui::CollapsingHeader("Transform Gizmo", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Enable Gizmo", &gizmoEnabled);
            ImGui::BeginDisabled(selectedTransform == nullptr || !gizmoEnabled);
            CoImGui::ComboBox("Operation", gizmoOperation);
            ImGui::Checkbox("Snap", &gizmoSnappingEnabled);
            if (gizmoSnappingEnabled) {
                switch (gizmoOperation) {
                case GizmoOperation::Translate:
                case GizmoOperation::Universal:
                    ImGui::DragFloat3("Translate Snap", &translateSnap.x, 0.01f, 0.001f, 100.0f);
                    translateSnap = glm::max(translateSnap, glm::vec3{0.001f});
                    break;
                case GizmoOperation::Rotate:
                    ImGui::DragFloat(
                        "Rotate Snap (deg)", &rotateSnapDegrees, 0.5f, 0.1f, 180.0f, "%.2f");
                    rotateSnapDegrees = std::max(rotateSnapDegrees, 0.1f);
                    break;
                case GizmoOperation::Scale:
                    ImGui::DragFloat("Scale Snap", &scaleSnap, 0.01f, 0.001f, 10.0f, "%.3f");
                    scaleSnap = std::max(scaleSnap, 0.001f);
                    break;
                }
            }
            ImGui::EndDisabled();

            if (selectedTransform == nullptr) {
                CoImGui::Text("Selected entity has no Transform.");
            }
        }

        for (const auto &editor : editorFunctions) {
            ImGui::PushID(editor.name.c_str());
            editor.callback(graph, selectedEntity);
            ImGui::PopID();
        }
    }

    void drawSelectedTransformGizmo(SceneGraph &graph) const
    {
        if (!gizmoEnabled || selectedEntity == 0) {
            return;
        }

        auto *transform = graph.getComponent<Components::Transform>(selectedEntity);
        if (transform == nullptr) {
            return;
        }

        const auto *viewport = ImGui::GetMainViewport();
        if (ImGui::GetCurrentContext() == nullptr) {
            return;
        }
        if (viewport == nullptr || viewport->Size.x <= 0.0f || viewport->Size.y <= 0.0f) {
            return;
        }

        const auto camera = findGizmoCamera(graph, *viewport);
        if (!camera.valid) {
            return;
        }

        const auto localMatrix = localMatrixFromTransform(*transform);
        const auto parentMatrix = transform->mode == Components::TransformMode::Local
                                      ? parentWorldMatrix(graph, selectedEntity)
                                      : glm::mat4{1.0f};

        glm::mat4 worldMatrix = transform->mode == Components::TransformMode::Local
                                    ? parentMatrix * localMatrix
                                    : localMatrix;

        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetRect(viewport->Pos.x, viewport->Pos.y, viewport->Size.x, viewport->Size.y);

        float snapValues[3]{};
        float *snapPtr = nullptr;
        if (gizmoSnappingEnabled) {
            switch (gizmoOperation) {
            case GizmoOperation::Translate:
            case GizmoOperation::Universal:
                snapValues[0] = translateSnap.x;
                snapValues[1] = translateSnap.y;
                snapValues[2] = translateSnap.z;
                snapPtr = snapValues;
                break;
            case GizmoOperation::Rotate:
                snapValues[0] = rotateSnapDegrees;
                snapPtr = snapValues;
                break;
            case GizmoOperation::Scale:
                snapValues[0] = scaleSnap;
                snapValues[1] = scaleSnap;
                snapValues[2] = scaleSnap;
                snapPtr = snapValues;
                break;
            }
        }

        ImGuizmo::PushID(static_cast<int>(selectedEntity));
        const auto mode =
            transform->mode == Components::TransformMode::Local ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
        if (ImGuizmo::Manipulate(glm::value_ptr(camera.view),
                                 glm::value_ptr(camera.projection),
                                 toImGuizmoOperation(gizmoOperation),
                                 mode,
                                 glm::value_ptr(worldMatrix),
                                 nullptr,
                                 snapPtr)) {
            glm::mat4 updatedLocal = worldMatrix;
            if (transform->mode == Components::TransformMode::Local) {
                const float determinant = glm::determinant(parentMatrix);
                if (std::abs(determinant) > std::numeric_limits<float>::epsilon()) {
                    updatedLocal = glm::inverse(parentMatrix) * worldMatrix;
                }
            }

            if (auto decomposed = decomposeTransform(updatedLocal, transform->scale)) {
                transform->position = decomposed->translation;
                transform->orientation = decomposed->orientation;
                transform->scale = decomposed->scale;
            }
        }
        ImGuizmo::PopID();
    }
};

ComponentEditorSystem::ComponentEditorSystem()
    : data_{std::make_unique<ComponentEditorPrivate>()}
{
    addComponentEditor("Transform", drawTransformEditor);
    addComponentEditor("Camera", drawCameraEditor);
    addComponentEditor("PointLight", drawPointLightEditor);
}

ComponentEditorSystem::~ComponentEditorSystem() {}

void ComponentEditorSystem::tick(SceneGraph &graph, TickInfo tickInfo)
{
    if (ImGui::GetCurrentContext() == nullptr) {
        return;
    }

    ImGui::ShowDemoWindow();
    if (ImGui::Begin("Scene Graph")) {
        // Available vertical space for the split area.
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const float treeHeight = avail.y * 0.4f; // 40% for the tree, 60% for the component editor

        ImGui::BeginChild("Tree", ImVec2(0, treeHeight));
        data_->drawEntity(graph, graph.root());
        ImGui::EndChild();

        ImGui::Separator();

        ImGui::BeginChild("Component Editor", ImVec2(0, 0));
        data_->drawSelectedComponents(graph);
        ImGui::EndChild();
    }
    ImGui::End();

    data_->drawSelectedTransformGizmo(graph);
}

void ComponentEditorSystem::addComponentEditor(std::string componentName,
                                               Function<void(SceneGraph &, Entity)> editorFunction)
{
    data_->editorFunctions.push_back({std::move(componentName), std::move(editorFunction)});
}
} // namespace Cory
