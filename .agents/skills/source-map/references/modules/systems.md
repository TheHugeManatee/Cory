# Systems

## Load When
- editing transforms, gizmos, or editor UI for components
- wiring scene data into rendering via system registration
- understanding how `BasicSystem` and `SystemCoordinator` drive the tick loop

## Main Paths
- `src/Cory/Systems/CommonComponents.hpp` — shared component types: `Transform`, `CameraComponent`, `PointLightComponent`; `TransformMode` coordinate space
- `src/Cory/Systems/SystemCoordinator.hpp` / `src/Cory/Systems/SystemCoordinator.cpp` — CRTP-based system registration; wraps each system in a proxy to store it in a vector, then forwards `tick()` calls
- `src/Cory/Systems/TransformSystem.hpp` / `src/Cory/Systems/TransformSystem.cpp` — computes `modelToWorld` from position/orientation/scale; normalizes orientation every frame; sorts scene graph by depth before updates
- `src/Cory/Systems/ImGuizmoTransformSystem.hpp` / `src/Cory/Systems/ImGuizmoTransformSystem.cpp` — extends transform editing with ImGuizmo manipulation; caches camera/projection and per-entity world matrices
- `src/Cory/Systems/ComponentEditorSystem.hpp` / `src/Cory/Systems/ComponentEditorSystem.cpp` — ImGui editor UI: tree view + component editors (Transform, Camera, PointLight) + gizmo overlay; extensible via `addComponentEditor()`

## Important Concepts
- `BasicSystem<Derived, Component...>` CRTP helper from `src/Cory/SceneGraph/System.hpp` lets a system declare which components it owns and provides `forEach<>`, `getComponent<>`, and `tick()` over the registry view
- `SystemCoordinator::emplace<Sys>(args...)` registers systems at construction; the coordinator then drives all registered systems via `tick(SceneGraph&, TickInfo)`
- `TransformMode` (Local vs World) determines whether a transform is relative to its parent or absolute in world space; ImGuizmo maps this to `ImGuizmo::LOCAL` / `ImGuizmo::WORLD`
- depth-first traversal plus sort-by-depth means transform updates are simple but potentially expensive on large scenes

## Read Next
- `src/Cory/SceneGraph/System.hpp` — defines `BasicSystem`, `SystemCoordinator`, and system registration helpers
- `examples/03-SceneGraph/src/SceneGraphDemo.cpp` — primary consumer wiring `ComponentEditorSystem` and `TransformSystem`
- `examples/06-VolumeRendering/src/VolumeRenderDemo.cpp` — consumer wiring `ComponentEditorSystem`, `ImGuizmoTransformSystem`, and `TransformSystem`

## Related Skills
- `cbt` — configure, build, run, test, lint/format Cory code

## Gotchas
- world matrix recomputation happens every frame and stale `unordered_map<Entity, glm::mat4>` entries are not pruned on entity removal
- `TransformSystem::beforeUpdate()` calls `sg.sortByDepth<Transform>()` every frame even when the hierarchy did not change
- both editor systems flip Vulkan Y (`projection[1][1] *= -1.0f`) to match ImGuizmo's OpenGL convention
- `SystemCoordinator::emplace()` wraps systems in a `detail::SystemImpl<Sys>` proxy instead of using virtual dispatch directly
