# Testing and Visual Review

## Load When
- adding or fixing tests
- running focused tests
- debugging visual regressions
- updating baselines or review artifacts

## Main Paths
- `tests/CMakeLists.txt` — defines `Cory_TestSupport`, `Cory_TestLib`, and `Cory_Tests`
- `tests/*.cpp` — Catch2 unit tests and test entry points
- `test-support/include/Cory/Testing/TestUtils.hpp` — Vulkan test harness / context helper
- `test-support/include/Cory/Testing/VisualTestUtils.hpp` — headless visual test canvas and compare helpers
- `test-support/src/TestUtils.cpp`
- `test-support/src/VisualTestUtils.cpp`
- `tests/baselines/visual/` — image baselines
- `tools/visual/CMakeLists.txt`
- `tools/visual/src/VisualReviewProtocol.cpp`
- `tools/visual/src/VisualReviewUi.cpp`
- `tools/visual/src/VisualDiffReviewer.cpp`
- `tools/visual/tests/VisualDiffReviewerUi_Test.cpp`

## Important Concepts
- Catch2 drives the unit-test stack
- `TestCanvas` and `ImGuiTestRenderer` provide headless rendering for visual tests
- visual compare flows produce actual/diff/request/decision artifacts
- the visual reviewer UI is a separate tool target built from `tools/visual/`

## Read Next
- `tests/CMakeLists.txt`
- `test-support/include/Cory/Testing/VisualTestUtils.hpp`
- `tools/visual/src/VisualReviewUi.cpp`
- `tests/baselines/visual/visual-diff-reviewer-ui.bmp`

## Related Skills
- `cbt` — configure/build/run/test/format workflows
- `cory-visual-testing` — create, debug, and update visual regression tests

## Gotchas
- visual failures should be reviewed, not blindly accepted
- use focused `./cbt test <regex>` runs instead of the full suite
- artifact and baseline paths matter when diagnosing render changes
- `03`, `05`, and `06` share code with the test stack
