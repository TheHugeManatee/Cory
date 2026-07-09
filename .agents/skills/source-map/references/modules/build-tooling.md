# Build and Tooling

## Load When
- building, running, testing, formatting, linting, or analyzing Cory
- checking toolchain state or build profile status
- inspecting example or test targets

## Main Paths
- `./cbt` — primary configure/build/run/test/format/analyze entry point (wrapper around CMake)
- `tools/cbt/` — the cbt script and supporting files
- `CMakeLists.txt` — top-level project setup, conan options, enable_testing(), `add_subdirectory(src)`, `add_subdirectory(tools)`, `add_subdirectory(tests)`, `add_subdirectory(examples)`, install rules
- `src/Cory/CMakeLists.txt` — builds `Cory::Cory` static library with all application sources; defines the main public target and export header
- `tests/CMakeLists.txt` — Catch2-based test infrastructure: `Cory_TestSupport`, `Cory_TestLib` object libs, executable `Cory_Tests` (discovered via `catch_discover_tests`)
- `examples/CMakeLists.txt` — adds example subdirectories in order: 01-HelloTriangle → 06-VolumeRendering
- `tools/visual/CMakeLists.txt` — builds static libs `CoryVisualReviewProtocol`, `CoryVisualReviewUi` (ALIAS targets `Cory::VisualReviewProtocol`, `Cory::VisualReviewUi`) and executable `VisualDiffReviewer`; uses CLI11

## Common Commands
- `./cbt start`
- `./cbt configure`
- `./cbt build`
- `./cbt test <regex>`
- `./cbt run <target> --frames <N>`
- `./cbt fmt`
- `./cbt analyze`
- `./cbt slang <shader>`
- `./cbt targets`
- `./cbt tests`
- `./cbt status`

## Related Skills
- `cbt` — configure/build/run/test/format/analyze workflow
- `slang` when shader compilation is involved (uses embedded Slang in src/Cory)

## Gotchas
- Do not call `cmake`, `ctest`, or built binaries directly unless explicitly requested.
- When running interactive targets, limit execution with `--frames`.
- Prefer focused tests over the full suite; use regex to filter by test name.
- Before changing C++ code, start with `./cbt start` to reset toolchain state.
