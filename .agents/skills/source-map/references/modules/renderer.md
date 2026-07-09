# Renderer

## Load When
- GPU resource creation, destruction, and lifetime management
- swapchain, frame source, upload, readback, sync, or command recording work
- shader manager, hot reload, compiler, or pipeline cache changes
- code touches KDGpu directly via `Gpu::` namespace alias

## Main Paths
- `src/Cory/Renderer/Context.hpp` — central service entry point; owns device setup (surface/headless), queues, resources, and sub-system accessors
- `src/Cory/Renderer/Gpu.hpp` — defines `using Gpu = KDGpu` + re-exports of handle types (`Device`, `Texture`, `CommandRecorder`, …) into the Cory namespace
- `src/Cory/Renderer/Common.hpp` — shared constants (`MAX_FRAMES_IN_FLIGHT=2`, `MAX_PUSH_CONSTANT_SIZE=128`), buffer usage / memory flag enums, `ShaderHandle` type alias
- `src/Cory/Renderer/Swapchain.hpp` — coroutine-based swapchain wrapper; manages image acquisition/presentation and per-frame resources (multisampled color+depth images, semaphores, fences); produces `FrameContext`
- `src/Cory/Renderer/FrameGenerator.hpp` — wraps a `cppcoro::generator<FrameContext>` into an iterator for frame consumption with automatic present-on-exit
- `src/Cory/Renderer/Synchronization.hpp` — wraps `simple_vulkan_synchronization`; provides `AccessType`, `ImageLayout`, `GlobalBarrier`, `BufferBarrier`, `ImageBarrier` for pipeline-barrier construction without raw Vulkan calls
- `src/Cory/Renderer/ShaderManager.hpp` — manages shader lifecycle with deferred release keyed on last-used frame; supports creation from file path or string source, both tracked by `std::source_location`
- `src/Cory/Renderer/AsyncUploader.hpp` — queue-family-aware staged upload service (buffer + image); supports handoff from transfer queue to graphics/consumer family via release/acquire barriers; returns `UploadTicket` with non-blocking `ready()` / blocking `wait()` semantics
- `src/Cory/Renderer/GpuBumpAllocator.hpp` — contiguous GPU memory allocator for zero-copy CPU↔GPU data; bump-allocation within a pre-allocated allocation

## Important Concepts
- **`Gpu::`** is the Cory namespace alias for KDGpu; all Vulkan resource types are accessed via `Gpu::Texture`, `Gpu::Buffer`, etc.
- **`Context`** aggregates device, queues (`graphicsQueue`, `computeQueue`, `transferQueue`), resources manager, pipeline cache, shader manager, descriptor sets, async uploader, thread scheduler
- **Frame acquisition**: `Swapchain` (windowed) and `HeadlessFrameSource` both implement the abstract `FrameSource` base; both use coroutines (`cppcoro::generator`) for frame generation
- **Queue-family handoff**: `AsyncUploader` detects when consumer family differs from upload family and emits release/acquire barriers automatically
- **Deferred shader release**: `ShaderManager::release()` accepts an optional last-used frame number so shaders survive until the next N frames (configurable via MAX_FRAMES_IN_FLIGHT)
- **Sync access model**: `Cory::Sync` collapses Vulkan's complex enum matrix into 40 usage types; map to Vulkan layouts via `GetVkImageLayout()` or use `Optimal` for automatic layout transitions

## Read Next
- `src/Cory/Renderer/Swapchain.hpp` — full frame lifecycle (acquire → command record → present)
- `src/Cory/Renderer/Context.hpp` — device and queue family index accessors (`graphicsQueueFamilyIndex()`, etc.)
- `examples/01-HelloTriangle/src/main.cpp` — minimal consumer using headless/windowed render paths

## Related Skills
- `kdgpu` — for KDGpu API details, handle types, resource creation patterns
- `slang` — for shader authoring and Slang compiler integration via `SlangCompiler.hpp`
- `cbt` — for configure/build/test/run/analyze/format workflow in this repo

## Gotchas
- **Resource lifetime**: respect buffer/image lifetimes while GPU work is in flight; destroying a resource before its fence completes yields undefined behavior.
- **KDGpu naming**: never invent API names from raw Vulkan — always use `Gpu::` re-exports or the full KDGpu types (`KDGpu::Texture`, etc.).
- **Push constant limit**: `MAX_PUSH_CONSTANT_SIZE = 128`; split large push data into multiple blocks.
- **AsyncUploader ownership**: `UploadTicket` uses `std::weak_ptr<SharedState>` internally — do not store the ticket longer than its fence completion; call `ticket.wait()` or `ready()` to finalize.
- **Swapchain presentation contract**: between `nextImage()` and `present()`, schedule work for the acquired semaphore, signal `rendered` after command submission, and signal `in_flight` at submit time — missing any step breaks the sync chain.
