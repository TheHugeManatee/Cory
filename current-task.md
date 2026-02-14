# Implement volume rendering

We implement direct volume rendering, with some extensions to make visual fidelity better.

# Plan

# Step 1:

Set up a basic volume raymarcher that can render a 3D texture using raymarching in a fragment shader.

- Create a 3D texture representing the volume data (e.g., a simple density field or a pre-defined dataset).
- Implement a compute shader that performs raymarching through the volume, accumulating color and opacity based on the
  sampled values.
- Set up a compute shader that performs the raymarching

# Step 2:

Extend the raymarcher to support basic lighting and transfer functions.

- Implement a simple lighting model (e.g., Phong or Blinn-Phong) to enhance the visual appearance of the volume.
- Introduce a transfer function that maps scalar values in the volume to color and opacity, allowing for better
  visualization of different structures within the volume.
- Add user controls to adjust the transfer function parameters in real-time.

# Step 3:

Temporal accumulation

- Add ray jittering to reduce banding artifacts
- Implement temporal accumulation to improve image quality over multiple frames

# Step 4:

Full Monte Carlo Volume Raycasting (optional, stretch goal)

- Implement Monte Carlo sampling techniques to improve the realism of the volume rendering.
- Explore advanced lighting models and scattering effects within the volume.
- Optimize performance to maintain interactive frame rates.

## Sidetracks

- Investigate async texture uploads for volume data (secondary)
- ✅ Investigate why the Framegraph dump does not work
- ✅ generalized shader hot reloading service
- ✅ standardized application run() function and tick handling

# Current Status

- Added 06-VolumeRendering as a new example project (will host most of the code for this feature before we lift this to
  library-level support)
- Add new VolumeRenderSystem that handles the volume rendering logic
- Added a compute subtask that raycasts the configured cubes and paints hits red using the existing instance buffer.
- Added comptue task to create a procedurally generated volume texture (3D) with some simple density functions.
- Added new async upload service to handle staging and uploading of large textures (like our volume) without blocking
  the main thread and queue (see ADR-04-async-upload.md for details.)
- Replaced the placeholder box-hit shader with a basic volume raymarching compute shader that samples the generated
  3D volume and composites density-driven color/opacity.
- Added an explicit sampled 3D volume binding in the raycast pass and wired it through `VolumeRenderSystem` using a
  dedicated linear-clamp sampler.
- Added disk-based hot reloading for VolumeRendering shaders (vertex, fragment, and both compute shaders) via
  `FileWatchManager`, with frame-safe deferred shader replacement.
- Added a second cube debug compute pass that raycasts instance boxes and writes local hit positions (mapped to RGB)
  to the output color buffer.
- Refactored volume shader hot-reloading into a reusable `Cory::ShaderHotReloader` utility and integrated it into
  `VolumeRenderSystem`.
- Split rasterization into an explicit `rasterizationTask` in `VolumeRenderSystem` and wired
  `VolumeRenderDemo::debugRasterize` to bypass compute/raycast passes when enabled.
- Implemented step 2 basics:
  - Added transfer-function mapping in `raymarch.comp.slang` (density window + gamma + opacity scale).
  - Added simple Blinn-Phong style lighting in the volume raymarcher using volume-gradient normals.
  - Added real-time ImGui controls in `VolumeRenderDemo` for transfer parameters (density min/max, opacity scale, gamma).
- Moved shader hot-reload processing for volume rendering into `VolumeRenderSystem::beforeUpdate` and passed frame
  numbers into `beforeUpdate` for all scene systems via `BasicSystem`, so reload processing is no longer duplicated
  across render tasks.
- Added a per-volume ImGui toggle to enable/disable raymarch jittering at runtime and plumbed it through
  `VolumeComponent` -> `InstanceData::raymarchParams`.
- Unified volume shader time inputs to use the application simulation clock value (`tick.now`) propagated from
  `VolumeRenderSystem` for both volume generation and raymarch/debug passes (removed frame-number-derived shader time).
- Implemented initial temporal accumulation (EMA) for the main volume raymarch path:
  - Added temporal blend factor controls in ImGui (enable/alpha/reset).
  - Added a persistent history texture allocated via `FramegraphResourceManager` and marked as an
    existing framegraph input each frame.
  - Updated raymarch shader to blend current frame with history using `temporalBlendFactor`.
  - Copied accumulated history back to the frame color target before layer rendering.
- Added a reusable `ImGuizmoTransformSystem` in Cory core systems and integrated it into `VolumeRenderDemo` so every
  entity with `Transform` gets an on-screen ImGuizmo manipulator each frame.
- Stabilized ImGuizmo transform round-tripping by decomposing edited local matrices with a Y-X-Z extractor that matches
  Cory's `makeTransform()` convention and unwrapping Euler angles to avoid frame-to-frame rotation/scale flipping.
- Centralized TRS math in `Cory::Math` (`eulerYXZToQuaternion`, `quaternionToEulerYXZ`, quaternion overload for
  `makeTransform`, and `decomposeTransform`) and switched `TransformSystem` + `ImGuizmoTransformSystem` to use it.
- Added quaternion-authoritative transform handling (`Transform::orientation`) with compatibility syncing for legacy
  Euler writes; during ImGuizmo drags we now update quaternion/scale/position continuously and only refresh Euler
  angles on interaction release to prevent jitter.
- Removed `Transform::rotation` entirely and migrated all transform updates to quaternion-only (`orientation`), including
  SceneGraph and VolumeRendering animation paths.
- Added a runtime `Show ImGuizmo` toggle in `VolumeRenderDemo` (default off) and wired it to
  `ImGuizmoTransformSystem::setEnabled()` for enabling/disabling gizmo rendering and interaction.
- Added focused math tests for TRS decomposition round-trip and Euler<->quaternion Y-X-Z conversion consistency.
