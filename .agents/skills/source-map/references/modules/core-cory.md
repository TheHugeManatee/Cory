# Core Cory Library

## Load When
- you need a broad overview of `src/Cory/`
- you need the owning boundary for a feature before choosing a narrower module
- you are unsure which subsystem to read first

## Main Paths
- `src/Cory/CMakeLists.txt` — defines the `Cory` static library target, source membership, dependency graph, and install/export rules
- `src/Cory/Cory.hpp` — public lifecycle API: `Cory::Init()` / `Cory::Shutdown()`
- `src/Cory/Cory.cpp` — lifecycle implementation; initializes AppClock, SimulationClock, Log, and FileWatchManager

## Important Concepts
- `Cory::Init()` / `Cory::Shutdown()` are the only top-level public library functions
- `Cory` is a static library aliased as `Cory::Cory`
- the library is split into 11 subsystem directories: Application, Base, Coro, Framegraph, ImGui, IO, Proper, Renderer, RenderTasks, SceneGraph, Systems
- public dependencies include KDGpu, Vulkan, Slang, GLFW, EnTT, glm, cppcoro, ImGui, spdlog, range-v3, magic_enum, efsw, and GSL

## Read Next
- `src/Cory/CMakeLists.txt` when target membership matters
- `.agents/skills/source-map/references/modules/<matching>.md` — read the module page for the subsystem you are editing

## Related Skills
- `cbt` — configure/build/run/test/analyze/format workflow
- `kdgpu` — KDGpu / KDGpuUtils API work inside renderer-facing code
- `slang` — shader authoring and compilation
- `cory-visual-testing` — visual regression tests driven by the renderer

## Gotchas
- this page is a router, not a symbol index
- the library exposes only two public symbols (`Init`, `Shutdown`)
- compile-time defines are set at the target level: `HAVE_CORY`, GLM defaults, and `CORY_DATA_DIR`
