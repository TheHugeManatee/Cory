# Task Router

Use the smallest module page that matches the task.

## Build, configure, run, test, lint, format, toolchain state
- Read: `modules/build-tooling.md`
- Use skill: `cbt`

## Repository-wide library ownership / where to start
- Read: `modules/core-cory.md`

## Application shell, windowing, layers, input, camera, ImGui layer
- Read: `modules/application.md`

## Shared utilities, logging, handles, timing, file watching, work contracts
- Read: `modules/base.md`

## Coroutines, synchronous waits, schedulers, thread pools, coroutine helpers
- Read: `modules/coro.md`

## Framegraph passes, transient resources, render-task wiring
- Read: `modules/framegraph.md`

## GPU resource creation/destruction, swapchain, uploads, sync, command recording
- Read: `modules/renderer.md`
- Use skill: `kdgpu`

## Shader sources, compilation, hot reload, shader manager
- Read: `modules/shaders.md`
- Use skill: `slang`

## UI rendering, widgets, ImGui integration
- Read: `modules/imgui.md`

## Scene graph, entities, transforms, editor/component systems
- Read: `modules/scenegraph.md`

## Property/parameter system
- Read: `modules/proper.md`

## Bitmap I/O and test image helpers
- Read: `modules/io.md`

## Prebuilt render task helpers
- Read: `modules/render-tasks.md`

## Runnable examples and minimal repros
- Read: `modules/examples.md`

## Tests, headless rendering, visual baselines, review artifacts
- Read: `modules/testing.md`
- Use skill: `cory-visual-testing`

## Updating or expanding the source map itself
- Read: `maintenance.md`
- Read: `module-template.md` if creating or refreshing a module page
- Then read the exact module page you are refreshing
- If the request is a full refresh, use sub-agents for module summaries instead of reading all modules directly in the coordinator context

## Escalation Rules
- Start narrow; do not read every module.
- If a task spans multiple areas, read the owning module first, then one adjacent module.
- When code touches external GPU APIs directly, combine `modules/renderer.md` with the `kdgpu` skill.
- When source-map work spans multiple modules, switch to a coordinator/sub-agent workflow before context becomes broad.
