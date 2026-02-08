Plan

Multi-Queue Execution Model: Timeline Queue Graph + Queue-Family Ownership
Summary
Implement a phased, production-safe multi-queue runtime for Cory’s framegraph using:

Explicit task queue intent with automatic fallback.
A compiled queue dependency graph.
Timeline semaphore synchronization across graphics/compute/transfer queues.
Correct queue-family ownership transfers for buffers/images.
Full compatibility fallback to current single-queue behavior.
This plan keeps current behavior stable while enabling incremental async compute/transfer adoption.

Decisions Locked
Scope: phased rollout with full infrastructure first, opt-in async usage.
Queue binding policy: explicit QueueDomain with default Auto.
Sync model: single global timeline semaphore + swapchain binary bridge.
Public API / Interface Changes
Common.hpp
Add enum class QueueDomain { Auto, Graphics, Compute, Transfer };
Common.hpp (RenderTaskInfo)
Add QueueDomain requestedQueue{QueueDomain::Auto};
Add QueueDomain resolvedQueue{QueueDomain::Graphics};
src/Cory/Framegraph/RenderTaskBuilder.hpp/.cpp
Add RenderTaskBuilder &queueDomain(QueueDomain domain);
Framegraph.hpp
Extend ExecutionInfo with queue submission telemetry:
std::vector<QueueSubmissionInfo> queueSubmissions;
std::vector<QueueDependencyInfo> queueDependencies;
src/Cory/Renderer/Context.hpp/.cpp
Add capability query helpers:
bool hasDedicatedComputeQueue() const;
bool hasDedicatedTransferQueue() const;
FrameContext.hpp
Add optional timeline bookkeeping for observability:
uint64_t timelineBeginValue{};
uint64_t timelineEndValue{};
Internal Architecture Changes
Add queue graph planner in framegraph compile pipeline.
Build per-task resolved queue.
Build inter-task edges and cross-queue dependency edges.
Produce queue-local execution slices with deterministic order.
Add timeline sync manager (new internal type, e.g. FramegraphQueueSync).
Own one timeline semaphore per context/device.
Allocate monotonically increasing values per submission.
Emit submit wait/signal sets.
Add multi-recorder execution path in Framegraph::record.
Record command buffers grouped by queue submission slice.
Submit queue slices in dependency order via timeline waits.
Add queue ownership transition generation in FramegraphResourceManager.
Track last queue-family owner per texture/buffer state.
Emit release barrier on source queue submission.
Emit acquire barrier on destination queue submission.
Skip ownership barriers when families are equal.
Add swapchain bridge logic in Swapchain::present path.
Wait on binary acquire semaphore only for first graphics submission touching swapchain.
Signal binary rendered semaphore from last graphics submission for present.
Keep existing present flow unchanged from caller perspective.
Queue Resolution Rules (Decision-Complete)
QueueDomain::Graphics -> graphics queue.
QueueDomain::Compute -> dedicated compute queue if available, else graphics queue.
QueueDomain::Transfer -> dedicated transfer queue if available, else graphics queue.
QueueDomain::Auto:
Render pass task -> graphics.
Compute pass with no graphics-only resource usage -> compute (if available), else graphics.
Copy/upload-only utility task -> transfer (if available), else graphics.
Any task that references present/swapchain outputs resolves to graphics.
Any unsupported placement falls back to graphics and records a debug trace.
Rollout Plan
Phase 1: Data model and queue planning.
Add API fields/methods and queue resolution.
Produce queue graph metadata in ExecutionInfo.
Keep execution on graphics queue only.
Phase 2: Timeline submission runtime.
Add timeline semaphore creation and per-frame value allocation.
Switch from single submit path to queue-slice submit plan.
Keep ownership transfers disabled but instrumented.
Phase 3: Ownership transfer correctness.
Enable release/acquire queue-family barriers for buffers/images.
Validate same-family fast path.
Phase 4: Opt-in async adoption in selected systems.
Start with compute-heavy passes (RadixSorter, particle/volume compute passes).
Gate with feature flag/config and collect telemetry.
Failure Modes / Fallback Behavior
If timeline semaphore support is unavailable: hard fallback to existing single-queue submit path.
If dedicated compute/transfer queue not found: resolve to graphics queue.
If ownership transfer generation fails invariant checks: assert in debug, fallback to same-queue path in release.
If queue graph has cycle (unexpected): fail compile with explicit task-edge diagnostics.
Test Plan
Unit: queue resolution matrix.
Inputs: task type + requested domain + device queue capabilities.
Expected: resolved queue domain.
Unit: queue graph dependency correctness.
Cross-queue producer/consumer chains produce expected wait edges.
Unit: timeline value monotonicity.
Submission plan values strictly increase and match dependency waits.
Unit: ownership transition generation.
Different families produce release+acquire pair.
Same family produces no ownership barriers.
Integration: headless async compute + graphics consume.
Validate output correctness and deterministic ordering.
Integration: swapchain present bridge.
Acquire/render/present semaphores remain valid and no regressions.
Regression: existing framegraph tests must pass unchanged in single-queue fallback mode.
Acceptance Criteria
cbt build and cbt test pass.
Single-queue behavior remains functionally identical by default.
At least one compute-heavy path executes on compute queue when available.
Queue-family ownership transfers are emitted only when required.
Framegraph dump/telemetry includes queue domain, submission order, timeline values, and cross-queue edges.
Assumptions And Defaults
Vulkan Sync2 and timeline semaphore support are available on target modern desktop GPUs; fallback is retained.
Initial public API surface is minimal: one task-level queue selector plus telemetry.
Multi-queue is opt-in by resolved placement rules and can be globally disabled via runtime flag if needed.
