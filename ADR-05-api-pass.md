# ADR-05 API Pass: Scoped Binding, Access Presets, Unified Frame Source

## Status
Accepted and implemented

## Context
This ADR documents the current API pass state in Cory for three closely related ergonomics and correctness goals:

1. Safer shader binding lifecycle via scoped ownership.
2. More concise and consistent resource dependency declarations.
3. A single frame-source contract for windowed and headless execution paths.

The implementation is additive and keeps low-level APIs available for explicit control.

## Decision Summary
Cory now uses the following architecture:

1. `ShaderBindingContext::ScopedBinding` as the preferred binding lifecycle API.
2. `RenderTaskBuilder` preset overloads as a concise layer over explicit `(usage, access)` declarations.
3. `FrameSource` as a unified frame lifecycle abstraction implemented by both `Window` and `HeadlessFrameSource`.

## Implemented Architecture

### 1) Scoped binding context (`ShaderBindingContext`)
`ShaderBindingContext` supports both manual binding and RAII-scoped binding.

Implemented API (header):
- `void bind(Gpu::RenderPassCommandRecorder &cmd);`
- `void bind(Gpu::RenderPassCommandRecorder &cmd, Gpu::PipelineLayoutHandle pipelineLayout);`
- `void bind(Gpu::ComputePassCommandRecorder &cmd);`
- `[[nodiscard]] ScopedBinding scoped(..., bool autoFlushOnExit = true);`
- `void flush();`
- `void unbind();`

`ScopedBinding` behavior:
- Created by `scoped(...)` after binding the pass recorder.
- Supports explicit `flush()`.
- Calls `unbind()` on destruction.
- Optionally auto-flushes on destruction (`autoFlushOnExit`, default `true`).
- Movable, non-copyable.

Current integration:
- `TransientRenderPass` and `TransientComputePass` hold a `std::optional<ShaderBindingContext::ScopedBinding>` and use scoped lifecycle management.

Additional current behavior:
- Per-frame descriptor dedup caches are active in `ShaderBindingContext`:
  - texture key: `(bindPoint, view, layout, sampler)`
  - sampler key: `(sampler)`
- Draw-data upload buffer supports introspection and resizing:
  - `drawDataBytesUsed()`, `drawDataBufferSize()`, `resizeDrawDataBuffer(...)`

### 2) Access preset layer (`RenderTaskBuilder`)
`RenderTaskBuilder` keeps explicit dependency APIs and adds preset overloads.

Implemented preset APIs:
- Texture read presets (`TextureReadPreset`):
  - `TransferSrc`
  - `ComputeSampled`
  - `FragmentSampled`
- Texture write presets (`TextureWritePreset`):
  - `TransferDst`
  - `ColorAttachment`
  - `DepthStencilAttachment`
  - `ComputeStorage`
- Texture read/write presets (`TextureReadWritePreset`):
  - `GeneralStorage`
  - `ColorAttachment`
  - `DepthStencilAttachment`
- Buffer read presets (`BufferReadPreset`):
  - `ComputeStorage`
  - `VertexStorage`
- Buffer write presets (`BufferWritePreset`):
  - `ComputeStorage`
- Buffer read/write presets (`BufferReadWritePreset`):
  - `ComputeStorage`

Implementation detail:
- Presets map in `src/Cory/Framegraph/Builder.cpp` to explicit `Gpu::*UsageFlags` and `Sync::AccessType` values.
- Unknown preset values assert (`CO_CORE_ASSERT(false, ...)`) to keep mapping exhaustive.

### 3) Unified frame source path (`FrameSource`)
`FrameSource` is now the shared runtime contract for frame production and output metadata.

Implemented interface (`src/Cory/Renderer/FrameSource.hpp`):
- `FrameGenerator frames()`
- `Gpu::Format colorFormat() const noexcept`
- `Gpu::Format depthFormat() const noexcept`
- `glm::u32vec2 extent() const noexcept`
- `Gpu::SampleCountFlagBits sampleCount() const noexcept`
- `size_t size() const noexcept`

Implementations:
- `Window : public FrameSource`
- `HeadlessFrameSource : public FrameSource`

Current usage in examples:
- `HelloTriangle`, `CubeDemo`, `SceneGraphDemo`, `DynamicPipeline`, `ParticleCompute`, `VolumeRendering`
  use a headless/window selection pattern and consume it via `FrameSource &`.

## Usage Samples

### Scoped binding in a pass
```cpp
auto bindingScope = shaderBindingContext.scoped(cmd, pipelineLayout);

auto tex = shaderBindingContext.bindTexture2D(colorHandle, Gpu::TextureLayout::ShaderReadOnlyOptimal);
auto samp = shaderBindingContext.bindSampler(defaultSampler);

shaderBindingContext.push(pushData);
bindingScope.flush();

cmd.draw(...);
// scope exit -> optional flush + unbind
```

### Preset-based dependency declaration
```cpp
auto sampled = builder.read(texture, RenderTaskBuilder::TextureReadPreset::FragmentSampled);
auto [outTex, outInfo] = builder.write(texture, RenderTaskBuilder::TextureWritePreset::ComputeStorage);
auto [rwTex, rwInfo] = builder.readWrite(outTex, RenderTaskBuilder::TextureReadWritePreset::GeneralStorage);

auto vertexIn = builder.read(buffer, RenderTaskBuilder::BufferReadPreset::VertexStorage);
auto [computeOut, computeOutInfo] = builder.write(buffer, RenderTaskBuilder::BufferWritePreset::ComputeStorage);
```

### Unified frame source selection
```cpp
FrameSource &frameSource = headless
    ? static_cast<FrameSource &>(*headlessFrames)
    : static_cast<FrameSource &>(*window);

for (auto &frameCtx : frameSource.frames()) {
    // common frame loop
}
```

## Validation and Current Test Coverage
Current tests include dedicated API-pass coverage in `tests/ApiPass_Test.cpp`:

1. Preset mapping correctness for texture/buffer read/write/readWrite.
2. Scoped binding lifecycle (scope exit unbind semantics).
3. Frame source contract checks for headless metadata and base-class conformance.

These run in the standard test suite and currently pass.

## Design Considerations

### Why this is additive
- Explicit low-level APIs remain available when exact synchronization/control is required.
- Presets are a convenience layer, not a hidden policy engine.

### Coroutine fit
- The API pass reduces imperative boilerplate in framegraph tasks without changing the coroutine-first task declaration model.
- `FrameSource::frames()` remains the shared coroutine-friendly source of `FrameContext`.

### Performance considerations
- Scoped binding mainly improves lifecycle safety and call-site consistency.
- Presets reduce declaration noise and mismatch risk; they do not change barrier semantics.
- Descriptor dedup and adaptive draw-data buffer sizing in `ShaderBindingContext` reduce per-frame descriptor/update pressure and fixed upload-buffer assumptions.

## Limitations and Non-Goals (Current State)
1. No framegraph scheduler redesign.
2. No queue-graph/timeline-sem integration as part of this ADR.
3. Preset coverage is intentionally limited to frequent patterns.
4. `FrameSource` unifies frame production/metadata; window-specific event APIs remain on `Window`.

## Migration Guidance for New Code
1. Prefer `scoped(...)` over manual `bind()/unbind()` in pass code.
2. Prefer preset overloads for standard read/write/readWrite declarations.
3. Use `FrameSource &` for shared headless/window frame loop logic.
4. Drop to explicit APIs when non-standard usage/access is required.

## References
- `src/Cory/Framegraph/ShaderBindingContext.hpp`
- `src/Cory/Framegraph/Builder.cpp`
- `src/Cory/Renderer/FrameSource.hpp`
- `src/Cory/Application/Window.hpp`
- `src/Cory/Renderer/HeadlessFrameSource.hpp`
- `tests/ApiPass_Test.cpp`
