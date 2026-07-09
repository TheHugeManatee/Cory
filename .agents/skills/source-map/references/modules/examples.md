# Examples

## Load When
- you need a runnable reference, an end-to-end usage example, or a likely repro target
- you want to see how Cory subsystems are composed in a real app

## Main Paths
- `examples/CMakeLists.txt` — registers the six example executables
- `examples/01-HelloTriangle/src/HelloTriangleApplication.cpp` — minimal application shell and rendering bootstrap
- `examples/02-CubeDemo/src/CubeDemo.cpp` — basic 3D demo reference
- `examples/03-SceneGraph/src/SceneGraphDemo.cpp` — scene graph + systems reference
- `examples/04-DynamicPipeline/src/DynamicPipelineApplication.cpp` — dynamic pipeline reference
- `examples/05-ParticleCompute/src/ParticleComputeDemo.cpp` — compute-focused example
- `examples/06-VolumeRendering/src/VolumeRenderDemo.cpp` — advanced rendering / volume path
- `examples/*/shaders/` — per-example shader sets

## Important Concepts
- examples are the fastest way to see subsystem usage end-to-end
- `03`, `05`, and `06` share code with the test stack
- some examples keep their own shader copies instead of reusing `data/shaders/`

## Read Next
- `examples/01-HelloTriangle/src/main.cpp` — minimal entry-point pattern
- `examples/03-SceneGraph/src/SceneGraphDemo.cpp` — scene graph + systems integration
- `examples/06-VolumeRendering/src/VolumeRenderDemo.cpp` — advanced rendering path

## Related Skills
- `renderer` — when examples touch GPU resource setup or frame scheduling
- `framegraph` — when examples wire declarative passes
- `shaders` — when example behavior is shader-driven
- `testing` — when an example doubles as a regression consumer

## Gotchas
- use `./cbt run <target> --frames N` for interactive examples
- shared code can overlap with tests; watch for duplicated helper logic
- per-example shaders can diverge from shared `data/shaders/` copies
