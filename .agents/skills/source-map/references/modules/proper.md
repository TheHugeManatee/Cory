# Proper — Property / Parameter System

## Load When
- editing or reading the coroutine-backed observable value system
- working on editor-facing numeric, enum, string parameters with validation
- debugging change-notification cancellation in coroutines (e.g. `co_await property.changed()`)

## Main Paths
- `src/Cory/Proper/Property.hpp` — basic `Property<T>` observable: `get/set/operator=`, `ChangedAwaiter`, coroutine-aware `changed()`
- `src/Cory/Proper/Properties.hpp` — variant types (`Int`, `Float`, `Vec3`, `String`), `AbstractProperty` waiter base, `Task<void>` cancellation plumbing
- `src/Cory/Proper/PropertySet.hpp` — named property storage + grouping via `SlotMap`; `PropertyProxy` for `[name]` access; typed change awaiters
- `src/Cory/Proper/Parameter.hpp` — named `Parameter<T>`, bounds-checked `NumericParameter<T>`, fixed-set `OptionParameter<T, Vals…>`, `StringOptionParameter`, and `EnumParameter` (magic_enum)

## Important Concepts
- **Observable value** — `Property<T>` is the source of truth; UI proxies read from it.
- **Coroutine-aware change notification** — `co_await property.changed()` suspends until a change happens; cancellation via `Task<void>::cancel()` stops further wakeups.
- **Variant dispatch** — internal storage uses `std::variant<Int, Float, Vec3, String>` and type erasure for cross-type updates in `PropertySet`.
- **Validation hierarchy** — `Parameter<T>` adds a name; `NumericParameter<T>` validates scalar + component-wise bounds (scalar or `glm::vec`); option/enum types reject values outside their constexpr allowed set.

## Read Next
- `src/Cory/Proper/Property.hpp` — basic observable flow and `ChangedAwaiter` registration
- `src/Cory/Proper/PropertySet.hpp` — named property storage and typed update flow
- `src/Cory/Proper/Parameter.hpp` — numeric, enum, option, and string-parameter validation
- `tests/Properties_Test.cpp` — full coverage: plain params, numeric bounds (incl. glm::vec3), option, enum, string option; change notification + cancellation

## Related Skills
- `coro` — coroutine primitives (`Task<void>`, `cppcoro`) used for awaitable change notifications
- `cbt` — configure/build/test/run workflows for the Proper target

## Gotchas
- **Waiters are cleared once** per `notifyWaiters()` call; coroutines must re-register themselves if they want to receive subsequent changes.
- **`NumericParameter::set(value)` returns `bool`** and silently rejects out-of-range values on assignment — check the return value or catch the stale old value.
- **`Task<void>` cancellation is part of the change-notification flow**: cancelling a waiter stops it from being woken by future `notifyWaiters()` calls; the destructor also cleans up.
- **Type mismatch in `PropertySet::update(handle, value)` triggers `CO_CORE_ASSERT(false)`** — always pass a matching variant type.
- **`EnumParameter` and `OptionParameter` derive accepted values at compile time** via `magic_enum::enum_values<T>()` or a constexpr array — invalid initial values assert at construction.
