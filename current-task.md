# New Property system

Property system based around coroutines
Build-system note:
 - Added configurable per-configuration sanitizer settings via CMake cache variables:
   `CORY_SANITIZERS_Debug`, `CORY_SANITIZERS_Release`, `CORY_SANITIZERS_RelWithDebInfo`,
   `CORY_SANITIZERS_MinSizeRel`.
 - Defaults are platform-aware: Windows Debug enables ASAN, Linux Debug enables ASAN+UBSAN, and
   non-Debug configs default to no sanitizers.
 - Sanitizer flags are applied through an internal interface target so installed/exported package
   consumers do not inherit the repo's sanitizer build settings automatically.
