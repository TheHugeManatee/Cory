---
name: kdgpu
description: Use when writing, reviewing, or debugging Cory code that calls KDGpu/KDGpuUtils APIs. Covers where to find KDGpu headers/examples, API entry points, and Cory-specific conventions such as using the Gpu namespace alias.
---

# KDGpu Usage for Cory

Use this skill when code touches KDGpu objects, options structs, command recording, synchronization, resource creation, uploads/readbacks, dynamic rendering, bind groups, or KDGpuUtils helpers.

## Cory conventions

- Cory aliases `KDGpu` as `Gpu` in `src/Cory/Renderer/Gpu.hpp`; prefer `Gpu::` in Cory code.
- Include `#include <Cory/Renderer/Gpu.hpp>` for common forward declarations/aliases; include concrete KDGpu headers only where full definitions are needed.
- Follow Cory style: designated initializers, `auto`, structured bindings, `std::span`/`std::vector`, and Cory logging/assert macros.
- Do not invent KDGpu API names from Vulkan names. Inspect the KDGpu header for the exact type/field name.

## Finding the right API

Primary local source checkout:

- Prefer `../KDGpu/src/` relative to Cory if present.
- Cory can also fetch KDGpu via `external/CMakeLists.txt`; local checkout selection uses `KDGPU_DIR`, `../KDGpu`, or `../kdgpu`.
- Build-generated copies may exist under `build/<profile>/_deps/kdgpu-build` or `build/<profile>/include`, but source headers are easier to inspect.

Public KDGpu headers live in `../KDGpu/src/KDGpu/`. Most APIs follow a predictable split:

- Resource class: `buffer.h`, `texture.h`, `command_recorder.h`, `queue.h`, etc.
- Creation/options struct: `buffer_options.h`, `texture_options.h`, `graphics_pipeline_options.h`, etc.
- Core enums/POD types: `gpu_core.h`.
- Handles: `handle.h`.
- Barriers: `memory_barrier.h`.
- Vulkan escape hatches: `vulkan/vulkan_resource_manager.h` and `vulkan/vulkan_*.h`.
- KDGpuUtils: `../KDGpu/src/KDGpuUtils/`, especially `resource_deleter.h`.

Useful lookup commands:

```bash
find ../KDGpu/src/KDGpu -maxdepth 2 -type f | sort
rg "struct TextureOptions|class CommandRecorder|TextureMemoryBarrierOptions" ../KDGpu/src/KDGpu
rg "copyTextureToBuffer|hostLayoutTransition|uploadTextureData" ../KDGpu/examples ../KDGpu/src/KDGpu
```

Useful examples:

- `../KDGpu/examples/offscreen_rendering/offscreen.cpp` — offscreen render, texture barriers, copy/readback, PNG write.
- `../KDGpu/examples/hello_triangle*` — minimal render setup.
- `../KDGpu/examples/render_to_texture*` — render targets.
- `../KDGpu/examples/textured_quad` — sampled texture/bind group basics.
- `../KDGpu/examples/compute_particles` — compute basics.
- `../KDGpu/examples/shader_object_dynamic_rendering` — shader-object/dynamic-rendering path.

## API shape

KDGpu is a thin Vulkan wrapper with move-only RAII resource classes and explicit synchronization.

Typical flow:

1. Use Cory `Context` to obtain `Gpu::Device`, queues, resource managers.
2. Create resources with `device.create*({...Options...})`.
3. Record commands with `device.createCommandRecorder()`.
4. Begin render/compute/raytracing pass as needed.
5. Bind pipeline/shaders/bind groups/buffers/push constants.
6. Draw/dispatch/copy/barrier.
7. End passes, `finish()` the command recorder, submit through `Gpu::Queue`.

Resource classes such as `Gpu::Buffer`, `Gpu::Texture`, `Gpu::TextureView`, `Gpu::Sampler`, `Gpu::CommandBuffer`, and `Gpu::Fence` are move-only and generally convertible to KDGpu handles. Use `isValid()` when needed.

## High-value headers / entry points

Resource creation:

- `device.h`: `Device::createBuffer`, `createTexture`, `createSampler`, `createShaderModule`, `createGraphicsPipeline`, `createComputePipeline`, `createCommandRecorder`, `createFence`, `createGpuSemaphore`, `createBindGroup*`.
- `buffer_options.h`: `Gpu::BufferOptions`.
- `texture_options.h`: `Gpu::TextureOptions`.
- `texture.h`: `Texture::createView`, `map`, `unmap`, `getSubresourceLayout`, host-copy helpers.
- `texture_view_options.h`, `sampler_options.h`.

Command recording:

- `command_recorder.h`: top-level copies, clears, barriers, pass begin, labels, `finish()`.
- `render_pass_command_recorder.h`: `setPipeline`, `bindShaders`, `setVertexBuffer(s)`, `setIndexBuffer`, `setBindGroup`, dynamic state, `pushConstant`, draw calls, `end()`.
- `compute_pass_command_recorder.h`: compute pass commands.
- `render_pass_command_recorder_options.h`: attachment/dynamic-rendering pass begin options.

Synchronization:

- `memory_barrier.h`: `TextureMemoryBarrierOptions`, `BufferMemoryBarrierOptions`, `MemoryBarrierOptions`.
- `queue.h`: `SubmitOptions`, `PresentOptions`, upload helpers.
- `fence.h`, `gpu_semaphore.h`.

Descriptors / bind groups:

- `bind_group_layout_options.h`: `BindGroupLayoutOptions`, `ResourceBindingLayout`.
- `bind_group_options.h`: `BindGroupOptions`, `BindGroupEntry`.
- `bind_group_description.h`: binding resource variants.
- `pipeline_layout_options.h`.

Pipelines / shaders:

- `graphics_pipeline_options.h`: `GraphicsPipelineOptions`, shader stages, vertex input, render target/depth/multisample/dynamic rendering options.
- `compute_pipeline_options.h`.
- `shader_object_options.h`: shader-object API used with `RenderPassCommandRecorder::bindShaders`.

## Dynamic rendering notes

Cory primarily uses dynamic rendering through the framegraph.

- For dynamic-rendering graphics pipelines, inspect `Gpu::GraphicsPipelineOptions::dynamicRendering` in `graphics_pipeline_options.h`.
- Attachment pass-begin layouts are in `render_pass_command_recorder_options.h`: `initialLayout`, `layout`, `finalLayout`.
- `initialLayout = TextureLayout::Undefined` discards previous contents; specify the real prior layout when preserving contents.

## Uploads and readback

Queue upload helpers in `queue.h`:

- `uploadBufferData(BufferUploadOptions)` / `waitForUploadBufferData(...)`.
- `uploadTextureData(TextureUploadOptions)` / `waitForUploadTextureData(...)`.
- Non-waiting helpers return `UploadStagingBuffer`; keep it alive until its fence signals.

In Cory, prefer existing services before direct queue uploads:

- `Cory::AsyncUploader` for staged async uploads.
- Framegraph resource management for render-task resources.
- `MappedCoherentDeviceBuffer` / framegraph draw-data allocation for transient CPU-to-GPU data.

Readback/screenshot references:

- Best starting point: `../KDGpu/examples/offscreen_rendering/offscreen.cpp`.
- Common KDGpu calls: `copyTextureToTexture`, `copyTextureToBuffer`, `Texture::getSubresourceLayout`, `Texture::map`, `Buffer::map`.
- Remember row pitch for linear texture readback.
- For Cory screenshots, prefer capturing a resolved single-sample color/swapchain image; use `resolveTexture` first if the source is multisampled.

## Cory-specific checks before adding raw KDGpu code

Before editing:

1. Locate the exact KDGpu header for the API/struct.
2. Search Cory for existing usage of the exact type/function.
3. Search KDGpu examples for the closest pattern.
4. Check whether Cory already wraps this concern in `Context`, `AsyncUploader`, `FramegraphResourceManager`, `ShaderManager`, `DescriptorSets`, `PipelineCache`, or `ShaderBindingContext`.

Be careful with:

- Resource lifetime while GPU work is in flight; use `KDGpuUtils::ResourceDeleter` or Cory frame-lifetime helpers when needed.
- Manual barriers inside framegraph tasks; Cory's framegraph may already own transitions.
- Native Vulkan access through `ctx.resources()` / `Gpu::VulkanResourceManager`; prefer KDGpu APIs unless an escape hatch is necessary.
- Feature-dependent APIs; Cory enables features in `src/Cory/Renderer/Context.*`.

After editing, validate through the `cbt` skill when allowed. For shader edits, also use the Slang skill and `./cbt slang <shader>`.
