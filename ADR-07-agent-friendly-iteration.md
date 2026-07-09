# ADR-07 Agent-Friendly Iteration and Visual Development

## Status
Proposed.

## Context
Cory is a Vulkan/KDGpu renderer and playground with a coroutine-oriented application loop, framegraph renderer, Slang shader workflow, examples, tests, and a custom `cbt` build tool. This review was a passive codebase inspection only; no build/test/run was performed.

The project is already relatively agent-friendly in several ways:

- Repository-level instructions are explicit in `AGENTS.md`.
- `./cbt` centralizes configure/build/test/run/format/lint/analyze/slang workflows.
- Project skills exist for `cbt` and Slang.
- A project-local Pi `subagent` extension exists under `.pi/extensions/subagent`.
- The renderer already has `HeadlessFrameSource`, `Context::setupHeadlessDevice()`, `Application::runMainLoop()`, frame limiting, and framegraph HTML dumping.
- Tests have a reusable headless GPU context via `tests/TestUtils.*`.

The biggest remaining gaps for fast agent iteration are:

1. No first-class screenshot/readback path from headless renders.
2. No standard visual acceptance-test contract.
3. Agent workflows still rely on general-purpose shell commands instead of project-aware tools.
4. Documentation is spread across README, ADRs, skills, CMake, and examples; there is no compact agent map of common change paths.
5. Subagent usage is possible but not yet specialized for Cory source surveys.

## Decision
Adopt an incremental “agent acceleration layer” around Cory rather than replacing existing workflows. The layer should consist of:

1. Engine support for deterministic headless snapshots.
2. `cbt` commands for screenshot capture and visual comparison.
3. Pi extension tools that wrap common Cory workflows with structured output.
4. Additional project skills for recurring agent tasks.
5. Short living documentation that gives agents stable navigation maps and accepted playbooks.

## Recommendations

### P0: Add headless screenshot capture

Implement a small rendering-capture API around `HeadlessFrameSource`.

Suggested engine pieces:

- `src/Cory/Renderer/FrameCapture.hpp/.cpp`
  - `captureColor(FrameContext&, Texture&) -> ImageRgba8` or writes directly to disk.
  - Internally records/executes `copyTextureToBuffer` or equivalent KDGpu readback, waits for a fence, maps staging/readback memory, handles row pitch, and normalizes to RGBA8.
- `src/Cory/IO/Image.hpp/.cpp`
  - `writePng(path, width, height, span<Rgba8>)` using a tiny dependency such as `stb_image_write`.
  - Optional `readPng` for tests.
- Extend `HeadlessFrameSource` with a safe way to access the last rendered swapchain/color image for capture.

Suggested CLI contract for examples:

```bash
./cbt run SceneGraphDemo --frames 1 -- --headless --screenshot out/scenegraph.png
```

Notes:

- Prefer capturing the resolved/copied single-sample swapchain image because the framegraph already copies to `frameHandles.swapchainImage` via `StandardRenderTasks::copyToTarget`.
- Make capture deterministic: fixed frame count, fixed resolution, fixed camera/seed where applicable, disable interactive UI in headless mode.
- Keep capture code independent from visual diffing so tests/tools can reuse it.

### P0: Add visual smoke tests

Add a small visual acceptance-test harness once screenshots exist.

Suggested structure:

- `tests/Visual/` or `tests/Visual_Test.cpp` for Catch2 integration.
- `tests/baselines/<target>/<case>.png` for golden images.
- `tests/artifacts/visual/<case>/actual.png`, `diff.png`, `metrics.json` for failures.
- A comparison helper that computes:
  - exact mismatch count,
  - max per-channel error,
  - mean absolute error,
  - optional small threshold for driver variance.

Suggested `cbt` command:

```bash
./cbt visual SceneGraphDemo --frames 3 --case scenegraph-default
./cbt visual --update-baseline SceneGraphDemo --case scenegraph-default
```

Start with one or two stable cases:

- `SceneGraphDemo --headless --frames 3`
- a clear-color/minimal framegraph case that isolates capture correctness.

### P1: Add Cory-specific Pi extension tools

Create `.pi/extensions/cory/index.ts` with project-aware tools. Keep each tool small and structured.

Recommended tools:

- `cory_status`
  - Reads `.cbt`/build config, reports active profile, build dir, known targets/tests, environment hints.
- `cory_targets`
  - Wraps `./cbt targets --json` and caches output for the session.
- `cory_tests`
  - Wraps `./cbt tests --json`, supports fuzzy filtering by source file or subsystem.
- `cory_run_test`
  - Runs one focused regex with timeout and returns parsed pass/fail summary.
- `cory_slang_check`
  - Wraps `./cbt slang`, infers related shader files, returns concise diagnostics.
- `cory_render_capture`
  - Wraps the proposed screenshot CLI and attaches/prints the output image path.
- `cory_visual_diff`
  - Compares image artifacts and returns metrics plus paths to actual/diff/baseline.
- `cory_map_symbol`
  - Uses `rg`, `compile_commands.json`, and CMake lists to answer “where is this class/test/target?” quickly.

Why extension tools help agents:

- They reduce token-heavy shell output.
- They encode project-specific safety rules such as frame limits.
- They return JSON-like summaries that are easier for models to act on.
- They can cache expensive discovery like test/target lists.

### P1: Add specialized skills

Existing skills cover `cbt` and Slang. Add skills for repeated high-level tasks:

1. `cory-architecture`
   - When to use: understanding or changing engine subsystems.
   - Include maps for `Application`, `Renderer`, `Framegraph`, `SceneGraph`, `Systems`, `Coro`, `IO`.
   - Include canonical edit paths and common pitfalls.

2. `cory-visual-dev`
   - When to use: screenshot capture, visual tests, rendering acceptance, image artifacts.
   - Include the exact `cbt visual`/capture commands once implemented.

3. `cory-framegraph`
   - When to use: adding render tasks, resources, transitions, framegraph dumps.
   - Include examples from `StandardRenderTasks` and `SceneGraphDemo`.

4. `cory-testing`
   - When to use: adding/repairing tests.
   - Include `VulkanTester`, focused Catch2 regex usage, artifact locations, and how to avoid slow full-suite runs.

### P1: Add an agent navigation map

Add `docs/agent-map.md` or expand `AGENTS.md` with a concise map:

- “If touching shaders, read `data/shaders`, example-local shaders, and use `./cbt slang`.”
- “If touching framegraph execution, inspect `Framegraph.*`, `RenderTaskBuilder.hpp`, `RenderTaskDeclaration.hpp`, and `StandardRenderTasks.*`.”
- “If touching examples, inspect their `main.cpp`, app class, shaders, and CMake target name.”
- “If touching GPU tests, use `tests/TestUtils.*` and focused `./cbt test` regex.”
- “If touching tools, use `tools/cbt/cbt/cli.py` and keep commands JSON-capable where useful.”

This should be optimized for agents, not humans: short, path-heavy, and task-oriented.

### P2: Improve iteration speed in `cbt`

Potential additions:

- `./cbt changed` — list source/test/shader files changed relative to a branch.
- `./cbt affected-tests <path>` — map a changed file to likely Catch2 regexes.
- `./cbt quick <path...>` — run format/check/compile/slang/test subset based on paths.
- `./cbt doctor --json`, `targets --json`, `tests --json` — continue expanding machine-readable outputs.
- `./cbt run` should optionally enforce/auto-add frame limits for known interactive targets.

### P2: Make examples more scriptable

Standardize common CLI options across examples:

- `--headless`
- `--frames N`
- `--width W --height H`
- `--screenshot PATH`
- `--dump-framegraph PATH_OR_PREFIX`
- `--seed N`
- `--no-imgui` or automatic no-UI in headless mode

This makes agent commands transferable between demos and reduces bespoke inspection.

### P2: Better observability artifacts

Add structured artifacts for rendering/debugging runs:

- framegraph dump path in JSON/log output,
- screenshot path,
- GPU adapter/features summary,
- validation errors as structured records,
- profiler records dumped to JSON after finite runs.

A useful command shape:

```bash
./cbt run SceneGraphDemo --frames 5 -- --headless --artifacts out/run-001 --report-json out/run-001/report.json
```

## Proposed implementation sequence

1. Create `docs/agent-map.md` and the new skills. This is low risk and immediately improves passive/active agent work.
2. Add a minimal `cory` Pi extension with read-only/discovery tools first (`cory_status`, `cory_targets`, `cory_tests`, `cory_map_symbol`).
3. Implement `FrameCapture` + PNG writing and expose `--screenshot` in `SceneGraphDemo`.
4. Add `./cbt visual` around screenshot capture and image diff.
5. Add visual smoke tests and baselines.
6. Expand capture support to `VolumeRenderDemo` and particle/compute examples.

## Consequences

Positive:

- Agents can visually inspect render output instead of relying only on logs.
- Rendering regressions become testable in CI and during agent sessions.
- Common workflows become faster, safer, and less token-heavy.
- New contributors and agents get stable maps of where to look.

Costs/risks:

- GPU screenshot tests can be flaky across drivers unless thresholds and deterministic scenes are used.
- Image baselines add binary files and review overhead.
- Pi extensions are executable code and should stay small, local, and auditable.
- `cbt` command growth should be kept cohesive; avoid turning it into a large test framework.

## Open questions

- Should screenshots use PNG only, or also raw RGBA/EXR for exact analysis?
- Where should visual baselines live if they become large: git, Git LFS, or generated fixtures?
- Should visual tests run by default, or only under an explicit `visual` label/profile?
- How much driver variance is acceptable for the first visual baselines?
