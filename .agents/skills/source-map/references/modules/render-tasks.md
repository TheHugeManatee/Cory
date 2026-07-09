# Render Tasks

## Load When
- editing small reusable framegraph task helpers
- wiring common clear/copy behavior into a pass graph

## Main Paths
- `src/Cory/RenderTasks/StandardRenderTasks.hpp` — declares `clearAttachments()` and `copyToTarget()` using `RenderTaskBuilder`
- `src/Cory/RenderTasks/StandardRenderTasks.cpp` — implementations; clear pass uses render-pass loadOp, copy auto-selects texture copy vs resolve

## Important Concepts
- both helpers compose with `Framegraph` via `RenderTaskBuilder`; `clearAttachments()` returns a `ClearPassOutputs` struct (color + optional depth)
- `copyToTarget()` picks the transfer op automatically: direct copy when sample counts match, or resolve when the source is multisampled and the target is single-sampled

## Read Next
- `src/Cory/Framegraph/RenderTaskBuilder.hpp` — the builder primitive used by both task declarations
- `tests/RenderTaskDeclaration_Test.cpp` — coverage for the declaration/build flow

## Related Skills
- `framegraph` — for pass graph and task composition work
- `renderer` — when touching GPU resource lifecycle tied to render tasks

## Gotchas
- `copyToTarget()` asserts same extent between source and target; size mismatch is rejected at runtime
- sample-count conversion in `copyToTarget()` only supports 1→1 or MSAA→single resolve
