# Coroutines

## Load When
- editing coroutine primitives, schedulers, thread pools, or sync-wait helpers
- adding async behavior outside the renderer framegraph
- wrapping awaitables for fail-fast execution against a stop source

## Main Paths
- `src/Cory/Coro/Coro.hpp` — `sync_wait()`, `EagerJob`, `LightweightManualResetEvent`, `SyncWaitTask`, and the `Awaitable` / `Scheduler` concepts
- `src/Cory/Coro/Coro.cpp` — platform-specific implementation for the sync-wait primitives
- `src/Cory/Coro/CoroThreadPool.hpp` / `src/Cory/Coro/CoroThreadPool.cpp` — worker-thread scheduler; `schedule()` returns an awaiter that resumes work on a pool thread
- `src/Cory/Coro/Scheduler.hpp` / `src/Cory/Coro/Scheduler.cpp` — `SyncScheduler` and `NextTickScheduler`
- `src/Cory/Coro/CoroOps.hpp` — `resume_on(pool, awaitable)` helper
- `src/Cory/Coro/CoroBatch.hpp` / `src/Cory/Coro/CoroBatch.cpp` — `when_all_fail_fast()` with stop-source coordination

## Important Concepts
- Cory uses cppcoro, not STL coroutines
- `sync_wait()` is a blocking bridge around `LightweightManualResetEvent`; it waits for coroutine final-suspend completion
- coroutine handles are destroyed explicitly in owning wrappers (`EagerJob`, `SyncWaitTask`)
- `CoroThreadPool` is non-copyable and non-movable

## Read Next
- `src/Cory/Renderer/ShaderHotReloader.*` — consumer using `EagerJob` for file watching
- `src/Cory/Application/DynamicGeometry.cpp` — consumer using `cppcoro::sync_wait()`
- `src/Cory/Base/WorkContractGroup.hpp` — scheduling cross-over

## Related Skills
- `cbt` — configure/build/run/test workflows for the project

## Gotchas
- `CoroThreadPool` rejects enqueue after shutdown has started
- `NextTickScheduler::tick()` drains all pending work in one call
- `LightweightManualResetEvent` uses platform-specific primitives (Linux futex, Win32 event, fallback mutex/CV)
