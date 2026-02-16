# ADR-04 Async Upload Service (Current State)

## Status
Accepted and implemented.

## Intent
Define the current upload architecture used by Cory for buffer/image transfers, including the
staging-slot model and the streamed volume ingestion path.

## Scope
`AsyncUploader` is an engine service owned by `Context` and intentionally orthogonal to framegraph.

- It does not register framegraph dependencies.
- It does not provide a queue-graph scheduler.
- It provides upload submission, completion tracking, and staging memory reuse.

## Implemented Components
- Uploader API: `src/Cory/Renderer/AsyncUploader.hpp`
- Uploader impl: `src/Cory/Renderer/AsyncUploader.cpp`
- Injectable staging interface: `src/Cory/Renderer/StagingUploader.hpp`
- Streamed volume loader (staged decode path): `examples/06-VolumeRendering/src/DatasetLoader.hpp`
- Streamed volume manager (partial staged uploads): `examples/06-VolumeRendering/src/VolumeManagerSystem.hpp`

## Architecture
### 1. Staging Ownership Model
Uploads use caller-provided staging via `StagingSlot`.

`StagingSlot`:
- carries staging buffer and byte capacity metadata
- is acquired from uploader pool and later recycled
- can be abstracted through `IStagingUploader` for testability

Key interfaces:
- `IStagingUploader::acquireStaging(byteSize) -> task<Result<StagingSlot>>`
- `IStagingUploader::mapStaging(slot) -> std::byte*`
- `IStagingUploader::unmapStaging(slot)`
- `IStagingUploader::recycleStaging(slot)`
- `IStagingUploader::validStaging(slot)`

`AsyncUploader` implements `IStagingUploader`.

### 2. Submission Model
Submission APIs are staged-only:
- `enqueueStagedBufferUpload(BufferUploadRequest, StagingSlot&&) -> UploadTicket`
- `enqueueStagedImageUpload(ImageUploadRequest, StagingSlot&&) -> UploadTicket`

Uploader records transfer commands and transitions/ownership barriers, submits work, and tracks
completion via fence-backed `UploadTicket`.

### 3. Completion/Reclamation Model
Each upload tracks:
- completion fence
- staging buffer
- upload command buffer
- optional queue-handoff semaphore/acquire command buffer (cross-family transfers)

On completion (`ready()/wait()` or `poll()`), uploader reclaims staging and in-flight records.

## Queue/Sync Policy
### Queue selection
- Use dedicated transfer queue if transfer family differs from graphics family.
- Otherwise use graphics queue.

### Consumer queue family
- Defaults to graphics queue family.
- Override allowed per request.

### Ownership
- Same-family: single submit with transfer->consumer barrier.
- Cross-family: release on upload queue, acquire on consumer queue with semaphore handoff.

## Streamed Volume Pipeline (Current)
`DatasetLoader` loads BMP stacks in parallel on cppcoro static thread pool and decodes directly
into acquired staging slots.

Flow:
1. Discover and order slices.
2. Worker coroutine acquires staging slot from `IStagingUploader`.
3. Worker decodes BMP directly into mapped staging memory.
4. Loader emits `StagedSliceLoadUpdate` callback.
5. `VolumeManagerSystem` submits partial depth=1 image uploads from staged slots.
6. `VolumeManagerSystem` tracks upload tickets and marks dataset `FullReady` when complete.

This path removes per-slice temporary decode vectors in streamed loading.

## Threading Contract
- BMP I/O + decode run on loader worker threads.
- Upload submission (`enqueueStagedImageUpload`) runs from manager tick/main thread.
- `poll()` is expected on main/render thread (`VolumeManagerSystem::tick` currently does this).
- `UploadTicket::ready()/wait()` are thread-safe for completion observation.

## API Usage (Canonical)
```cpp
auto slotResult = co_await uploader.acquireStaging(byteSize);
if (!slotResult) {
    co_return std::unexpected(slotResult.error());
}

auto slot = std::move(*slotResult);
auto* dst = uploader.mapStaging(slot);
// Fill dst[0..byteSize)
uploader.unmapStaging(slot);

auto ticket = uploader.enqueueStagedImageUpload(Cory::AsyncUploader::ImageUploadRequest{
    .destinationTexture = texture.handle(),
    .byteSize = byteSize,
    .regions = std::move(regions),
    .oldLayout = Gpu::TextureLayout::Undefined,
    .finalLayout = Gpu::TextureLayout::ShaderReadOnlyOptimal,
    .finalStages = Gpu::PipelineStageFlagBit::ComputeShaderBit,
    .finalMask = Gpu::AccessFlagBit::ShaderReadBit,
}, std::move(slot));
```

## Operational Notes
### Correctness
- Ticket completion is the CPU-visible completion contract.
- Callers must set correct destination stage/access masks.
- Cross-family ownership transfer is explicit in submitted barriers.

### Performance
- Staging pool uses best-fit reuse to reduce allocation churn.
- Streamed volume path supports partial uploads and first-slice renderability.
- Throughput and total load stats are logged by `VolumeManagerSystem` on completion.

### Testability
- `DatasetLoader` depends on `IStagingUploader`, enabling pure unit tests with fake staging
  allocators and callback-failure injection without GPU context.

## Known Limitations
- Completion backend is fence-based.
- `co_await UploadTicket` blocks in `await_suspend` (no async continuation scheduler yet).
- Uploader remains framegraph-agnostic; ordering with framegraph resources is caller-managed.
- `acquireStaging()` is coroutine-shaped but currently immediate (no awaitable backpressure queue).

## Non-Goals
- No framegraph dependency integration.
- No timeline semaphore DAG orchestration.
- No dedicated uploader thread.

