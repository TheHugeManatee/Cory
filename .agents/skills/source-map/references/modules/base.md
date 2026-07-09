# Base

## Load When
- editing shared utilities that do not belong to a rendering subsystem
- working on logging, handles, timing, file watching, slot maps, or work contracts

## Main Paths
- `src/Cory/Base/Common.hpp` — `NoCopy`, `NoMove`, `SlotMap`/`SlotMapHandle` forward decls, `Span<T>` alias, `Color`, `FileWatch` types; shared includes for fmt/GSL/magic_enum/concepts
- `src/Cory/Base/Log.hpp` — spdlog-based logging with `CO_CORE_*` / `CO_APP_*` macros and `AssertionFailed`; scoped level helpers (`SetCoreLevelScoped`)
- `src/Cory/Base/Time.hpp` — `AppClock` wrapper around `high_resolution_clock`, `Seconds`/`Timepoint` aliases, system-clock conversions
- `src/Cory/Base/SimulationClock.hpp` — `BasicSimulationClock<Upstream>` template with configurable time scale and pause; global clock via `SimulationClock::now()` / `tick()`
- `src/Cory/Base/Result.hpp` — `std::expected<T, std::string>` alias (`using Result = ...`)
- `src/Cory/Base/SlotMap.hpp` + `src/Cory/Base/SlotMapHandle.hpp` — chunked associative container with versioned handles; iterators and `cppcoro::generator` over handles/items
- `src/Cory/Base/SignalTree.hpp` — lockfree threadsafe binary signal tree (set/select/count); used internally by work contracts
- `src/Cory/Base/WorkContractGroup.hpp` — reschedulable work tokens via `WorkContractGroup` / `WorkContract` / `ContractToken`; uses `SignalTree` internally
- `src/Cory/Base/FileWatchManager.hpp` + `FileWatchManager.cpp` — shared file-watch singleton with typed handles (`FileWatchHandle`)
- `src/Cory/Base/ResourceLocator.hpp` + `ResourceLocator.cpp` — resource lookup helper
- `src/Cory/Base/Debugger.hpp` + `Debugger.cpp` — debugger integration
- `src/Cory/Base/Utils.hpp` + `Utils.cpp` — `formatBytes`, `readFile`, `lambda_visitor`, `stbi_image` wrapper
- `src/Cory/Base/Math.hpp` — spherical to cartesian conversion, transform helpers (`makeTransform`, `decomposeTransform`, ortho/perspective), angle unwrapping, quaternion to Euler (YXZ)
- `src/Cory/Base/Random.hpp` — random number utilities
- `src/Cory/Base/FmtUtils.hpp` — fmt-based formatting helpers
- `src/Cory/Base/Function.hpp` — callable wrappers / function objects
- `src/Cory/Base/Locked.hpp` — RAII-locked access to a value with mutable proxy (`lock()` returns `*object_`)
- `src/Cory/Base/BitField.hpp` — bitfield template for enum types (set/clear/toggle/query/set_bits)
- `src/Cory/Base/EnumMap.hpp` — compile-time-indexed map from scoped enum to value type U; `forEach` ordered by enum key
- `src/Cory/Base/ValOptional.hpp` — sentinel-value alternative to `std::optional`; throws on invalid access via `BadValueOptional`

## Important Concepts
- logging macros live in `Base/Log.hpp`.
- `NoCopy` and `NoMove` are the common non-copyable/non-movable tags.
- `SlotMap` + versioned typed handles back several engine containers.
- `AppClock` / `SimulationClock` provide time helpers; simulation clock must be `tick()`'d to advance.
- `FileWatchManager` is the shared file-watch singleton with coroutine-aware scheduling.
- `WorkContractGroup` models reschedulable work tokens using a lockfree signal tree.

## Read Next
- `src/Cory/Base/Common.hpp` — shared type aliases and tags
- `src/Cory/Base/Log.hpp` — logging macros and diagnostics
- `src/Cory/Base/Utils.hpp` — file/format helpers and small utilities

## Related Skills
- `cbt` — build/test/run workflows for Cory

## Gotchas
- `SlotMap` handles carry a version; stale handles throw on use.
- `SimulationClock::now()` implicitly ticks the global clock.
- `ValOptional` uses a sentinel value (default: max) instead of `std::nullopt_t`; throws on access when invalid.
- Keep `FileWatchManager` and `WorkContractGroup` coroutine-aware scheduling in mind.
- Prefer existing `CO_CORE_*` / `CO_APP_*` logging/assert macros over direct spdlog calls.
