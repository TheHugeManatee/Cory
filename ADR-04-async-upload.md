# ADR-04 Async Upload Service (Orthogonal To Framegraph)

## Status
Accepted and implemented.

## Purpose
Document the current architecture and usage of Cory's async upload subsystem.

This ADR supersedes the initial "implementation plan" style write-up and now describes what exists in code today, how to use it correctly, and what design constraints still apply.

## Context
`ADR-03-queue-sync.md` describes a broader framegraph-integrated multi-queue execution model. For current needs, that model was too large in scope.

The immediate requirement was an upload mechanism that can:
- queue bulk buffer/image transfers without immediate blocking waits,
- leverage dedicated transfer queue hardware where available,
- safely hand off ownership to the queue family that will consume uploaded resources,
- remain orthogonal to framegraph scheduling.

## Decision Summary
Cory uses a standalone `AsyncUploader` service owned by `Context`.

- Entry point: `Context::uploader()`.
- Completion model: `UploadTicket` (`ready()`, `wait()`, `co_await`).
- Queue policy: transfer queue preferred, graphics fallback.
- Cross-family ownership: explicit release/acquire barriers only when needed.
- Sync implementation: fence-backed completion with binary semaphore bridging for cross-queue handoff.

## Current Implementation Footprint
- API: `src/Cory/Renderer/AsyncUploader.hpp`
- Implementation: `src/Cory/Renderer/AsyncUploader.cpp`
- Context integration: `src/Cory/Renderer/Context.hpp`, `src/Cory/Renderer/Context.cpp`
- Initial migrated call sites:
  - `src/Cory/Application/DynamicGeometry.cpp`
  - `examples/01-HelloTriangle/src/HelloTriangleApplication.cpp`
- Unit coverage:
  - `tests/AsyncUploader_Test.cpp`

## Architecture
### Service Boundary
`AsyncUploader` is deliberately outside framegraph.

It does not register dependencies in framegraph or participate in framegraph scheduling phases. It provides resource upload work submission and completion tracking as an independent runtime service.

### Core Types
- `BufferUploadRequest`
  - destination buffer handle
  - source data pointer and size
  - destination offset
  - destination access scope (`dstStages`, `dstMask`)
  - optional consumer queue family

- `ImageUploadRequest`
  - destination texture handle
  - source data pointer and size
  - copy regions and optional subresource range
  - `oldLayout -> TransferDstOptimal -> finalLayout`
  - final access scope (`finalStages`, `finalMask`)
  - optional consumer queue family

- `UploadBatchRequest`
  - spans of buffer and image requests

- `UploadTicket`
  - `ready()`: non-blocking completion probe
  - `wait()`: blocking completion wait
  - `co_await`: coroutine awaiter (currently blocks in `await_suspend`)

### Internal Resource Model
The uploader keeps per-upload state in an in-flight record and reclaims it when completion is detected.

Tracked per upload:
- completion fence
- staging buffer
- upload command buffer
- optional acquire command buffer (cross-family case)
- optional queue-handoff semaphore (cross-family case)

Staging memory is pooled and reused with a best-fit strategy to reduce churn and avoid repeated allocations for common upload sizes.

### Queue Selection and Ownership
Upload queue:
- use transfer queue when transfer and graphics queue families differ,
- otherwise use graphics queue.

Consumer queue family:
- defaults to graphics queue family,
- can be overridden per request.

Ownership behavior:
- same-family: single submission path with standard transfer->consumer barrier.
- cross-family: two-submit path:
  1. release barrier on upload queue + signal handoff semaphore,
  2. acquire barrier on consumer queue + signal completion fence.

## Execution Flow
### Buffer Upload (same queue family)
1. Poll and reclaim any completed in-flight uploads.
2. Acquire/reuse staging buffer.
3. Copy CPU data into staging buffer.
4. Record copy + transfer-to-consumer barrier.
5. Submit once and signal completion fence.
6. Return ticket.

### Buffer Upload (cross queue family)
1. Steps 1-4 as above, but record release barrier with queue family transfer.
2. Submit upload commands on upload queue, signal handoff semaphore.
3. Record acquire barrier on consumer queue.
4. Submit acquire commands waiting on handoff semaphore, signal completion fence.
5. Return ticket.

### Image Upload
Same high-level flow as buffer upload, with explicit layout transition pipeline:
- transition to `TransferDstOptimal`,
- copy staging buffer to texture,
- transition to final layout and release/acquire ownership as needed.

## API Usage
### Minimal Buffer Upload
```cpp
auto ticket = ctx.uploader().enqueueBufferUpload(Cory::AsyncUploader::BufferUploadRequest{
    .destinationBuffer = vertexBuffer.handle(),
    .data = vertices.data(),
    .byteSize = vertices.size() * sizeof(Vertex),
    .dstOffset = 0,
    .dstStages = Gpu::PipelineStageFlagBit::VertexAttributeInputBit,
    .dstMask = Gpu::AccessFlagBit::VertexAttributeReadBit,
});

// Optional: block only when needed.
ticket.wait();
```

### Fire-and-poll Pattern
```cpp
auto ticket = ctx.uploader().enqueueBufferUpload(request);

// Later in update/render loop
ctx.uploader().poll();
if (ticket.ready()) {
    // Safe to use uploaded resource under declared dst stage/access.
}
```

### Coroutine Await
```cpp
auto ticket = ctx.uploader().enqueueImageUpload(imageRequest);
co_await ticket;
// Upload complete.
```

### Cross-Queue Consumer Family
```cpp
auto ticket = ctx.uploader().enqueueBufferUpload(Cory::AsyncUploader::BufferUploadRequest{
    .destinationBuffer = buffer.handle(),
    .data = bytes.data(),
    .byteSize = bytes.size(),
    .dstStages = Gpu::PipelineStageFlagBit::ComputeShaderBit,
    .dstMask = Gpu::AccessFlagBit::ShaderReadBit,
    .consumerQueueFamilyIndex = ctx.computeQueueFamilyIndex(),
});
```

## Operational Considerations
### Correctness and Synchronization
- The ticket completion point is the synchronization contract for CPU-side flow.
- Resource consumers must respect the declared destination stage/access fields.
- For cross-family transfers, ownership handoff is encoded by explicit release/acquire barriers.

### Performance
- Throughput improves by removing per-upload immediate waits.
- Staging pool reuse reduces allocation cost for repeated upload workloads.
- Enqueue paths do not poll implicitly; callers must either consume tickets (`ready()/wait()/co_await`) or call `poll()` periodically to reclaim completed upload resources.

### Coroutines
- `co_await UploadTicket` is supported for API consistency.
- Current await implementation is synchronous in `await_suspend`; it does not schedule continuation from a background uploader thread.

### Framegraph Interaction
- The uploader is not framegraph-aware.
- If uploads feed framegraph resources, callers must ensure ordering externally (ticket wait/readiness checks at safe boundaries).

## Non-goals (Current State)
- No framegraph queue-graph integration.
- No timeline-semaphore DAG submission model.
- No dedicated uploader thread.

## Known Limitations
- Completion backend is fence-based in current implementation.
- Awaiting a ticket blocks instead of yielding to a dedicated async executor.
- Upload dependencies are not automatically represented in framegraph execution metadata.

## Future Evolution
Possible next steps (not part of current behavior):
- timeline semaphore-based completion and richer completion batching,
- framegraph-visible upload dependency integration,
- optional dedicated upload worker thread or scheduler integration,
- transfer command coalescing for very high upload counts.

## Extension Plan: Staging-Slot Decode Pipeline (In Progress)
### Motivation
Volume stack loading currently decodes each BMP slice into a transient CPU `std::vector<std::byte>`
and then copies into uploader staging memory. For large stacks this creates allocator pressure and
an extra CPU copy in the hot path.

### Decision
Introduce a staging-slot workflow in `AsyncUploader` and use it for streamed volume loading:
- worker coroutines acquire reusable uploader staging slots,
- worker coroutines decode BMP bytes directly into mapped staging memory,
- render-thread-side system submits pre-filled staging slots for GPU copy.

This removes per-slice temporary vector allocation from the streamed loading path and eliminates
the extra copy from decode buffer to staging buffer.

### API Direction
Planned/introduced uploader APIs:
- `acquireImageStaging(byteSize) -> task<Result<ImageStagingSlot>>`
- `recycleImageStaging(ImageStagingSlot&&)`
- `enqueueStagedImageUpload(ImageUploadRequest, ImageStagingSlot&&) -> UploadTicket`

`ImageStagingSlot` is a move-only staging ownership token carrying:
- staging buffer,
- byte capacity/size metadata.

### Thread-Boundary Contract
- `acquireImageStaging()` is safe from loader worker threads.
- BMP I/O + decode always run on worker threads.
- `enqueueStagedImageUpload()` is called by `VolumeManagerSystem` during tick (render/main thread),
  preserving command submission locality and existing queue usage policy.
- Upload completion and staging reclamation remain fence/ticket-driven.

### Rollout Notes
- Keep existing `enqueueImageUpload(data=...)` for existing call sites.
- Migrate only volume streamed path first (`DatasetLoader` + `VolumeManagerSystem`).
- Follow-up work can add bounded async backpressure and explicit scheduler handoff APIs if needed.
