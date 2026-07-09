# Framegraph

## Load When
- adding or changing render passes
- debugging transient resources or transitions
- wiring a render task into graph execution

## Main Paths
- `src/Cory/Framegraph/Framegraph.hpp` — graph orchestration and execution entry points
- `src/Cory/Framegraph/Builder.cpp` — framegraph builder implementation
- `src/Cory/Framegraph/FramegraphResourceManager.hpp` / `src/Cory/Framegraph/FramegraphResourceManager.cpp` — resource lifetime and transient allocation management
- `src/Cory/Framegraph/RenderTaskBuilder.hpp` — builder primitive used by render-task declarations
- `src/Cory/Framegraph/RenderTaskDeclaration.hpp` — task declaration surface used by passes and systems
- `src/Cory/Framegraph/TransientRenderPass.hpp` / `src/Cory/Framegraph/TransientComputePass.hpp` — transient pass wrappers
- `src/Cory/Framegraph/ShaderBindingContext.hpp` — shader binding helper for framegraph execution
- `src/Cory/Framegraph/FramegraphVisualizer.h` — framegraph visualizer hooks
- `src/Cory/RenderTasks/StandardRenderTasks.*` — reusable clear/copy task helpers

## Important Concepts
- declarative render-task description
- automatic resource creation and transition handling
- transient render and compute pass wrappers
- imported frame-context resources
- execution planning via `ExecutionInfo`

## Read Next
- `src/Cory/Framegraph/Framegraph.hpp`
- `src/Cory/Framegraph/RenderTaskBuilder.hpp`
- `tests/FrameGraph_Test.cpp`

## Related Skills
- `renderer` — when touching GPU resource lifecycle or frame resources
- `shaders` — when framegraph passes bind shader resources
- `coro` — for cppcoro async execution within render tasks
- `cbt` — configure/build/test/run workflows

## Gotchas
- `record()` is a one-shot execution path per framegraph instance
- `RenderTaskDeclaration` coroutines may never resume if the pass is not needed
- call `resetForNextFrame()` when reusing frame slots
- `ScopedBinding` auto-flushes on exit by default; use `release()` to defer
- `TransientTextureHandle` versioning uses `handle + N` for dependency tracking
