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

## Agent Notes
- Prefer `Gpu::` over `KDGpu::` inside Cory code.
- Prefer coroutine-aware design for foundational async behavior.
- Use `./cbt` rather than direct `cmake` or `ctest`.

## Fill-In Targets
- subsystem ownership hints
- common editing journeys
- historical caveats or invariants
