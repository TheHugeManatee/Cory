**ADR 03 — Multi-Queue Execution Model: Queue Synchronization and Ownership**

**Status:** Proposed, on hold

**Date:** 2026-02-15

**Context**

Cory's Framegraph currently submits all work to a single queue. To enable better utilization of modern GPUs and CPUs, we need a production-safe model to schedule tasks across multiple Vulkan queues (graphics/compute/transfer) while preserving correctness, observability and a safe fallback to the existing single-queue behavior.

**Decision**

Implement a phased multi-queue runtime that provides:

- Explicit per-task queue intent with an `Auto` fallback.
- A compiled queue dependency graph used to order cross-queue submissions.
- Timeline semaphore synchronization for inter-queue waits.
- Correct queue-family ownership transfers for buffers/images where necessary.
- Full compatibility fallback to the current single-queue submit path.

This will be rolled out incrementally so existing behavior remains stable while enabling opt-in async compute/transfer adoption.

**Public API / Interface Changes**

- Add `enum class QueueDomain { Auto, Graphics, Compute, Transfer };` in `Common.hpp`.
- Extend `RenderTaskInfo` with `requestedQueue` and `resolvedQueue` fields:

	- `QueueDomain requestedQueue{QueueDomain::Auto};`
	- `QueueDomain resolvedQueue{QueueDomain::Graphics};`

- Add builder API: `RenderTaskBuilder &queueDomain(QueueDomain domain);`.
- Extend `ExecutionInfo` with telemetry containers:

	- `std::vector<QueueSubmissionInfo> queueSubmissions;`
	- `std::vector<QueueDependencyInfo> queueDependencies;`

- Add capability queries to `Context`:

	- `bool hasDedicatedComputeQueue() const;`
	- `bool hasDedicatedTransferQueue() const;`

- Add optional timeline bookkeeping to `FrameContext`:

	- `uint64_t timelineBeginValue{};`
	- `uint64_t timelineEndValue{};`

These surface-level changes are intentionally minimal to keep the public API small.

**Internal Architecture Changes**

- Add a queue graph planner in the framegraph compilation pipeline that:
	- Resolves each task to a concrete queue.
	- Builds cross-task edges (inter-queue dependencies).
	- Produces queue-local execution slices with deterministic ordering.

- Add a timeline sync manager (e.g. `FramegraphQueueSync`) responsible for:
	- Owning one timeline semaphore per context/device.
	- Allocating monotonically increasing values per submission.
	- Emitting per-submit wait and signal sets.

- Add multi-recorder execution path in `Framegraph::record`:
	- Record command buffers grouped by queue submission slice.
	- Submit queue slices in dependency order using timeline waits.

- Enhance `FramegraphResourceManager` to generate queue ownership transitions:
	- Track last queue-family owner for resources.
	- Emit release barriers on the source submission and acquire barriers on the destination submission.
	- Skip barriers when queue-families are equal.

- Add swapchain bridge logic in `Swapchain::present`:
	- Wait on a binary acquire semaphore only for the first graphics submission that touches the swapchain.
	- Signal a binary rendered semaphore from the last graphics submission before present.

Existing external behavior for present calls remains unchanged.

**Queue Resolution Rules (Decision-Complete)**

- `QueueDomain::Graphics` -> graphics queue.
- `QueueDomain::Compute` -> dedicated compute queue if available, else graphics queue.
- `QueueDomain::Transfer` -> dedicated transfer queue if available, else graphics queue.
- `QueueDomain::Auto` rules:
	- Render pass task -> graphics.
	- Compute pass with no graphics-only resource usage -> compute (if available), else graphics.
	- Copy/upload-only utility task -> transfer (if available), else graphics.
	- Tasks referencing present/swapchain outputs resolve to graphics.
	- Unsupported placements or inability to satisfy placement fall back to graphics and emit a debug trace.

**Rollout Plan**

Phase 1 — Data model and planning

- Add API fields and queue resolution logic.
- Produce queue graph metadata in `ExecutionInfo`.
- Keep runtime behavior on graphics queue only.

Phase 2 — Timeline submission runtime

- Create timeline semaphores and per-frame value allocation.
- Switch from single-submit path to queue-slice submit plan (still disabled for ownership transfers).

Phase 3 — Ownership transfer correctness

- Enable release/acquire queue-family barriers for buffers/images.
- Validate same-family fast path and correctness tests.

Phase 4 — Opt-in async adoption

- Enable opt-in compute/transfer placement for selected systems (RadixSorter, particle/volume compute passes).
- Gate with a feature flag and collect telemetry.

**Failure Modes / Fallback Behavior**

- If timeline semaphores are unavailable: fallback to the existing single-queue submit path.
- If no dedicated compute/transfer queue is present: resolve tasks to the graphics queue.
- If ownership transfer generation fails invariant checks: assert in debug; in release, fall back to same-queue behavior for safety.
- If the queue graph contains a cycle: fail compilation with explicit diagnostics (cycles should be impossible under correct task declaration).

**Test Plan**

- Unit tests
	- Queue resolution matrix: verify resolved queue for combinations of task type, requested domain and device capabilities.
	- Queue graph dependency correctness: verify produced wait edges for cross-queue producer/consumer chains.
	- Timeline monotonicity: ensure submission values strictly increase and waits match signals.
	- Ownership transitions: ensure different families generate release+acquire; same family generates none.

- Integration tests
	- Headless async compute + graphics consume: validate deterministic outputs.
	- Swapchain present bridge: validate acquire/render/present semaphore correctness.

- Regression
	- Existing framegraph tests must pass in single-queue fallback mode.

**Acceptance Criteria**

- `./cbt build` and `./cbt test` pass.
- Default single-queue behavior is unchanged.
- At least one compute-heavy path executes on a dedicated compute queue when available.
- Queue-family ownership transfers are emitted only when required.
- Framegraph dumps and telemetry include queue domain, submission order, timeline values, and cross-queue edges.

**Assumptions & Defaults**

- Target platforms generally support Vulkan Sync2 and timeline semaphores; a hard fallback is retained.
- The initial public surface remains minimal (one task-level queue selector plus telemetry).
- Multi-queue support is opt-in and globally defeatable via runtime configuration.

**Consequences**

- Increased runtime complexity in the framegraph and resource manager.
- Improved GPU/CPU utilization for compute- and transfer-heavy workloads on capable devices.
- Need for observability and telemetry to validate placement and performance gains.

**References**

- Vulkan Sync2 / timeline semaphores
- Swapchain acquire/present synchronization patterns

**Authors**

- (You) — design & implementation owner

**Next Steps**

1. Implement queue resolution & compilation metadata (Phase 1).
2. Add timeline semaphore manager and submission logic (Phase 2).
3. Implement resource ownership transitions and validation (Phase 3).
4. Enable opt-in passes and collect telemetry (Phase 4).
