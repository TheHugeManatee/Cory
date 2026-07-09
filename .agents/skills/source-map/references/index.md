# Cory Source Map

## Purpose
Compact router for Cory tasks. Read `routes.md`, then one matching module page, then the exact source files named there.

## Start Here
1. Read `routes.md`.
2. Read the smallest matching file under `modules/`.
3. Read the source files named there.
4. If refreshing the map itself, read `maintenance.md` first.
5. If refreshing a module page, use `module-template.md`.

## Top-Level Layout
- `src/Cory/` — main `Cory` static library
- `examples/` — runnable reference apps
- `tests/`, `test-support/`, `tests/baselines/visual/` — unit and visual test stack
- `tools/visual/` — visual diff reviewer used by visual tests
- `tools/cbt/` — `./cbt` implementation
- `data/shaders/`, `examples/*/shaders/` — shared and example-local Slang shaders
- `.agents/skills/` — repo-local skills

## Core Library Subsystems
- `Application/`, `Base/`, `Coro/`, `Framegraph/`, `ImGui/`, `IO/`, `Proper/`, `Renderer/`, `RenderTasks/`, `SceneGraph/`, `Systems/`

## Primary Skills
- `source-map`
- `cbt`
- `kdgpu`
- `slang`
- `cory-visual-testing`

## Important Entry Points
- `src/Cory/CMakeLists.txt`
- `src/Cory/Cory.hpp`
- `src/Cory/Cory.cpp`
- `src/Cory/Renderer/Context.hpp`
- `src/Cory/Framegraph/Framegraph.hpp`
- `src/Cory/SceneGraph/SceneGraph.hpp`
- `src/Cory/Systems/SystemCoordinator.hpp`
- `tests/CMakeLists.txt`
- `examples/CMakeLists.txt`

## Next Files
- `routes.md`
- `repo-overview.md`
- `maintenance.md`
- `module-template.md`
