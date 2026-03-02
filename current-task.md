# New Property system

Property system based around coroutines

Basic premises:
 - Properties are simple structs
 - Avoid heap allocations as much as possible (use std::pmr and arenas where needed)

Parameter layer note:
 - Added a named `Parameter<T>` wrapper on top of `Property<T>` so higher-level UI/config values can carry an explicit identifier without losing the existing property await/get/set behavior.
 - Added `NumericParameter<T>` with optional per-instance `min`/`max` bounds; this works for both scalar values and `glm` vector types, with vector bounds enforced component-wise.
 - Validation-based parameter setters are non-throwing and return `bool`, so invalid UI edits can be rejected without exceptions while leaving the existing value unchanged.
 - `CoImGui` property editors are no longer tied to `kdb::Property`; they now work with any holder exposing `get()` and `set(...)`, which makes the new Proper property/parameter types usable directly in existing UI code.
 - Added enum-specific `EnumParameter<E>` that derives its accepted values from `magic_enum::enum_values<E>()` and validates assignments with `magic_enum::enum_contains`.

Build-system note:
 - Added configurable per-configuration sanitizer settings via CMake cache variables:
   `CORY_SANITIZERS_Debug`, `CORY_SANITIZERS_Release`, `CORY_SANITIZERS_RelWithDebInfo`,
   `CORY_SANITIZERS_MinSizeRel`.
 - Defaults are platform-aware: Windows Debug enables ASAN, Linux Debug enables ASAN+UBSAN, and
   non-Debug configs default to no sanitizers.
 - Sanitizer flags are applied through an internal interface target so installed/exported package
   consumers do not inherit the repo's sanitizer build settings automatically.
 - `./cbt` now resolves the active sanitizer set before Conan/CMake, stores it in build config,
   injects it back into CMake via `CORY_ACTIVE_SANITIZERS`, and uses
   `CORY_SANITIZERS_VIA_TOOLCHAIN` to avoid double-applying flags.
 - `./cbt` uses sanitizer-specific `CONAN_HOME` directories so ASAN and non-ASAN dependency
   binaries do not mix; this is required for Windows/MSVC ASAN because linked static libraries
   like Catch2 must be built with matching sanitizer settings.
 - On Windows/MSVC, sanitizer flags from Conan land in `CMAKE_*_FLAGS_INIT`, so existing CMake
   caches can retain non-ASAN compile/link flags even after `cbt` switches the profile to an ASAN
   Conan home. `cbt` now detects this stale cache state and resets the CMake configure artifacts
   before rebuilding, which preserves STL vector/string annotations by keeping the app and Conan
   dependencies on the same ASAN configuration.
 - `./cbt configure` and `./cbt reconfigure` now expose explicit `--sanitizers` and
   `--no-sanitizers` switches so the active sanitizer set is synced into both the stored profile
   state and the corresponding `CORY_SANITIZERS_<build-type>` CMake define.
 - Direct non-`cbt` sanitizer builds are not considered supported, especially on Windows where
   Conan dependency variants must match the active sanitizer set.
