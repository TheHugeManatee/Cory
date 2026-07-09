# Repository Overview

## What Cory Is
Cory is a modern C++ Vulkan renderer/playground with modular subsystems, example apps, visual testing, and coroutine-oriented engine design.

## Structural Summary
- Main reusable code lives in `src/Cory/` as the `Cory` static library.
- End-user and reference applications live in `examples/`.
- Tests and support libraries live in `tests/` and `test-support/`.
- Visual review tooling lives in `tools/visual/` and `tests/baselines/visual/`.
- Build/test/run workflows go through `./cbt`.

## Build Graph Highlights
- Root `CMakeLists.txt` adds `external/`, `src/`, `tools/`, `tests/`, and `examples/`.
- `src/Cory/CMakeLists.txt` defines the `Cory` static library.
- `tests/CMakeLists.txt` defines `Cory_TestSupport`, `Cory_TestLib`, and `Cory_Tests`.
- `tools/visual/CMakeLists.txt` defines `CoryVisualReviewProtocol`, `CoryVisualReviewUi`, and `VisualDiffReviewer`.
- `examples/CMakeLists.txt` adds the six example executables: `HelloTriangle`, `CubeDemo`, `SceneGraphDemo`, `DynamicPipeline`, `ParticleComputeDemo`, and `VolumeRendering`.

## External Technology Anchors
- Vulkan
- KDGpu / KDGpuUtils
- Slang
- GLFW
- cppcoro
- EnTT
- ImGui

## Ownership Hints
- `Application/` — shell, layers, input, camera, and ImGui bridge code.
- `Base/` — logging, timing, file watch, containers, result helpers, and shared utilities.
- `Coro/` — cppcoro-based schedulers, sync-wait helpers, and coroutine batch helpers.
- `Framegraph/` — declarative render-task wiring and transient resource orchestration.
- `ImGui/` — UI rendering, input widgets, and ImGui layer integration.
- `IO/` — BMP query/decode/write helpers used by tests and visual tooling.
- `Proper/` — observable property and parameter validation layer.
- `Renderer/` — context, swapchain, upload, shader, sync, and pipeline management.
- `RenderTasks/` — prebuilt framegraph task helpers.
- `SceneGraph/` — entity hierarchy and traversal.
- `Systems/` — transform, gizmo, and component-editor systems built on the scene graph.
- `tests/`, `test-support/`, and `tools/visual/` — unit tests, headless canvas, and visual review flow.

## Common Editing Journeys
- `HelloTriangle` for bootstrap and window/render-loop wiring.
- `SceneGraphDemo` for hierarchy and systems behavior.
- `VolumeRendering` for shader, render-task, and data-flow changes.
- `tests/*` and `test-support/*` when a change needs regression coverage.

## Agent Notes
- Prefer `Gpu::` over `KDGpu::` inside Cory code.
- Prefer coroutine-aware design for foundational async behavior.
- Use `./cbt` rather than direct `cmake` or `ctest`.
