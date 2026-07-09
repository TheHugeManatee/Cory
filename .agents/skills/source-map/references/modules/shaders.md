# Shaders and Slang

## Load When
- editing `.slang` files (data/, examples/*/shaders/)
- working on shader compilation, push constant reflection, or hot reload
- following shader-related build failures

## Main Paths
- `data/shaders/FullscreenTriangle.vert.slang` — fullscreen debug triangle overlay vertex
- `data/shaders/DepthDebug.frag.slang` — depth debug fragment; imports `CoryBindings`
- `data/shaders/CoryBindings.slang` — shared module: descriptor set/binding constants for bindless textures (set 1)
- `data/shaders/RadixSortHistogram.comp.slang`, `RadixSortScan.comp.slang`, `RadixSortScatter.comp.slang` — radix sort pipeline compute shaders
- `examples/*/shaders/` — per-example shader sets; each example has its own copy (e.g. 02-CubeDemo, 03-SceneGraph both have cube.vert/frag)
- `src/Cory/Renderer/SlangCompiler.*` — wraps Slang COM session; lazy global singleton, SPIR-V 1.5, column-major matrices
- `src/Cory/Renderer/ShaderHotReloader.*` — file-watch async hot reload via `FileWatchManager`; coroutine-driven watch loop
- `src/Cory/Renderer/ShaderManager.*` — central manager: create/release shaders with deferred release keyed by frame number

## Important Concepts
- **ShaderSource** loads from file or inline string; stage inferred from extension (.vert/.frag/.geom/.comp) if not specified via `deduceTypeFromPath()`
- **Runtime Slang → SPIR-V**: compilation happens at runtime through a global session singleton (`detail::getGlobalSession()`); SPIR-V 1.5, column-major matrix layout, debug info disabled by default
- **Push constant reflection** captured during compile for root constant detection; `Shader.usesRootConstant()` and `pushConstantSize()` expose the result
- **Deferred release**: `ShaderManager.release(handle)` with no frame number releases immediately; otherwise deferred until currentFrame + MAX_FRAMES_IN_FLIGHT (`clearDeferredReleases`)
- **HotReloader** uses `FileWatchManager` coroutine-based async event loop; calls `processPendingReloads(frameNumber)` each frame on the render thread

## Read Next
- `src/Cory/Renderer/Shader.cpp` — ShaderSource construction, CompileToSpv, SPIR-V → shader object pipeline
- `src/Cory/Renderer/SlangCompiler.cpp` — Slang session init, compile request, push constant reflection extraction
- `data/shaders/CoryBindings.slang` — shared bindless resource declarations used by debug and volume shaders

## Related Skills
- `slang` — shader authoring and fixes
- `kdgpu` — KDGpu shader object / pipeline layout APIs
- `renderer` — renderer subsystem context

## Gotchas
- **Slang session is a global singleton** (`detail::getGlobalSession()`); do not instantiate multiple `SlangCompiler`s concurrently.
- **"Warnings as errors"**: non-empty diagnostic output from Slang means compilation failure; empty diagnostics + valid SPIR-V only on success.
- **HotReloader requires `initialize(ctx)` before `addShader`**; the destructor calls `shutdown()` which releases all watches and handles.
- Example shaders under `data/shaders/` are shared across examples (e.g. `DepthDebug.frag.slang`, `FullscreenTriangle.vert.slang`). Each example has its own copy in `examples/*/shaders/` that may differ from the data versions.
