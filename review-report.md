# Review report: agent iteration and visual testing changes

## Scope

Reviewed the complete repository diff from
`ba821d736cb2845268a3e126c2ae1f0204f9bd9e` through `f89ab639`, including all 43
changed files, the eight commits in that range, ADR-07, ADR-08, build integration,
BMP IO, test support, visual capture/comparison, the native reviewer, and both Pi
extensions. Uncommitted fixes made during this review are included below.

## Fixes made during review

- Fixed `getTestContext()` initializing the static object being constructed instead
  of the local `Context`. This was use-before-initialization and prevented reliable
  headless test setup.
- Made visual-review environment mutation portable and scoped. The test suite now
  builds on Linux instead of calling Windows-only `_putenv_s`, and previous
  environment values are restored after each test.
- Replaced the raw Vulkan texture readback call with KDGpu
  `CommandRecorder::copyTextureToBuffer()` and invalidated CPU-visible memory before
  mapping it. This keeps the test harness on the project abstraction and supports
  non-coherent host memory.
- Hardened RGBA BMP decoding/writing against `INT32_MIN`, arithmetic overflow,
  invalid/zero dimensions, truncated offsets, oversized files, and paths without a
  parent directory.
- Replaced hand-built metrics JSON with `nlohmann::json`. `CORY_VISUAL_RESULT` is
  now valid escaped single-line JSON suitable for machine parsing.
- Implemented the ADR-08 environment contracts:
  `CORY_VISUAL_BASELINE_DIR`, `CORY_VISUAL_ARTIFACT_DIR`,
  `CORY_UPDATE_VISUAL_BASELINES`, and `CORY_VISUAL_ALWAYS_WRITE_ACTUAL`. Added a
  focused regression test for path overrides and baseline update mode.
- Made the reviewer UI golden fixture platform-independent by removing absolute
  temporary/source paths from fixture content. Regenerated and visually inspected
  the baseline; the test now uses a small per-channel tolerance for llvmpipe versus
  hardware renderer UI rasterization differences.
- Replaced the reviewer preview's per-pixel `AddRectFilled()` rendering with uploaded
  RGBA textures and one `ImGui::Image()` quad per preview. A 512x512 visible image
  previously emitted more than one million vertices and could exceed ImGui's
  16-bit draw-list index range while scrolling or zooming, causing an assertion and
  process abort.
- Added general ImGui texture registration and per-command texture binding to the
  KDGpu renderer. The backend now also honors ImGui's `IdxOffset`/`VtxOffset`,
  advertises large-mesh support, and assigns an explicit font-atlas texture ID.
- Removed duplicate baseline mutation from the native reviewer UI. The comparison
  process now updates a baseline only after reading a valid, matching acceptance
  decision.
- Made protocol writes fail loudly, accepted only the declared v1 schemas, and
  rejected empty request/decision identifiers.
- Hoisted shared texture readback into core as `Cory::readbackTextureRgba8` and
  wired CubeDemo to write a BMP via `--output` in headless mode.
- Fixed a sanitizer-reported null dereference in `SlangCompilerTools`: a
  `TypeLayoutReflection` may exist without an underlying `TypeReflection`, so its
  convenience `getName()` cannot be called unconditionally.
- Fixed `cbt` bootstrap regressions: use `$HOME`, prefer `python3` when creating a
  fresh Linux venv, invoke the venv interpreter consistently, and stop immediately
  with an actionable error when the venv is absent.
- Fixed the subagent extension's undefined `estimateTokens()` call, configuration
  precedence (bundled defaults no longer override user/project settings), unexpected
  signal exit status, and the watchdog path that could return while leaving an
  orphan process. Corrected its README to match actual rendering/config behavior.
- Fixed the visual-review extension's `contextLines` option, which previously
  returned at most 11 lines even when up to 60 were requested.
- Removed stray text/trailing whitespace from ADR-07.

## Verification

- `./cbt start`: passes.
- `./cbt build --target Cory_Tests --jobs 2`: passes with ASan/UBSan enabled.
- 14 focused BMP, `VulkanTester`, `TestCanvas`, comparison, and reviewer protocol
  tests: pass.
- Reviewer UI visual golden test: passes with the UI-specific tolerance
  (`perChannelTolerance=2`, `maxMeanAbsoluteError=0.01`).
- The reviewer UI test now uses 512x512 previews at 1x zoom. This exceeds the old
  rectangle renderer's draw-list capacity and directly guards against the abort.
- Five focused reviewer/TestCanvas/interactive visual tests pass after the texture
  renderer change.
- CubeDemo now supports headless BMP export via `--output`; a one-frame run wrote
  `/tmp/cory-cubedemo-output.bmp`, then failed process teardown due an existing
  LeakSanitizer report in KDGui/XCB clipboard initialization.
- Environment override/baseline update regression test: passes.
- Targeted `./cbt lint` completed successfully. It reports substantial existing
  warning noise, primarily include-cleaner/style diagnostics and third-party
  warnings; it is not currently useful as a clean quality gate.
- `git diff --check`: clean after the fixes.
- Pi TypeScript execution/type-checking was not available in this environment; the
  repository has no local extension package manifest or type-check command.

## ADR progress

### ADR-08: TestCanvas visual testing

Phase 1 is substantially present: a one-frame headless canvas, clear-color capture,
RGBA normalization, BMP baselines, strict comparison metrics, failure artifacts,
machine-readable output, a native/agent review protocol, and UI regression coverage.

Remaining important gaps:

1. Add `./cbt visual-test` with JSON output, update-baseline handling, deterministic
   artifact roots, timeouts, and parsing of `CORY_VISUAL_RESULT`.
2. Add the planned `cory_visual_test` Pi tool. The current extension reviews an
   already-created request but cannot discover or run visual tests.
3. Add the `cory-visual-testing` skill and concise workflow documentation.
4. Add shader/triangle and reusable render-task visual tests. Current rendering
   coverage is a clear and the reviewer UI; it does not yet protect pipeline,
   viewport, vertex, or shader behavior.
5. Add optional framegraph HTML output on failure.
6. Move from BMP to PNG (or support both). The UI baseline is 5.4 MB uncompressed;
   PNG would materially reduce repository and artifact size.
7. Validate `ImageRgba8` dimensions/buffer size at the public comparison boundary
   and return a structured failure instead of relying on internal assertions.
8. Decide and encode the CI policy. Strict clear-color tests are portable; complex
   shader/UI baselines will eventually need platform baselines or justified
   tolerances.

### ADR-07: Agent-friendly iteration

The new harness closes the most important test-only screenshot/readback gap, and the
review tool gives agents image/source context. The broader ADR remains mostly open:

1. Promote stable capture/readback code from test support into an engine
   `FrameCapture` API. (Core helper now exists; CubeDemo is the first consumer.)
2. Add deterministic screenshot CLI support to at least one example, then expose it
   through `cbt`. (CubeDemo now supports `--output`; broader rollout remains.)
3. Add project-aware discovery/run tools (`cory_status`, target/test discovery,
   symbol mapping, focused run helpers).
4. Add the proposed agent navigation map and architecture/framegraph/testing skills.
5. Standardize headless example arguments and structured artifact reports.

## Further findings and recommendations

### High priority

- Add automated checks for `.pi/extensions/**/*.ts`. The missing
  `estimateTokens()` made the subagent tool fail as soon as it rendered its first
  update, and no repository check caught it. A minimal package/lockfile plus
  type-check and smoke tests should be part of `cbt`.
- Finish hardening `visual_review_request` mutation paths. Artifact and decision
  paths are now constrained to the request directory, and accepted baseline updates
  are constrained to the current working directory, but the tool should eventually
  use an explicit repo/baseline root instead of relying on `process.cwd()`.
- Make artifact publication atomic. Write JSON/BMP files to a sibling temporary
  path and rename after successful close so agents never observe partial request,
  decision, or baseline files.

### Medium priority

- `immer` currently adds a fetched dependency and CMake warning surface for one
  unused `VisualReviewQueue` type. Remove it until a persistent queue is implemented,
  or isolate it privately so it does not leak through
  `Cory::VisualReviewProtocol`.
- Artifact directories are keyed by test/case/source line, not request ID. Concurrent
  runs of the same comparison can overwrite each other. Include a run/request
  component while retaining a stable “latest” pointer if desired.
- The native reviewer loads malformed/missing images as unavailable but reports only
  generic UI state. Surface decode errors in the request UI and process exit output.
- Add protocol unit tests for malformed schema, truncated JSON, write failure,
  mismatched decision IDs, rejection, and relative paths.
- The shipped subagent model map is installation-specific
  (`local-openai/ornith-1.0-9b-mtp`). Keep the bundled file as an example or provide
  a clear first-run diagnostic instead of assuming that provider exists.

### Build/quality observations

- Configuring `immer` changes the dependency project's C++ standard and emits a
  warning during Cory configuration. Confirm that target-level settings cannot leak
  into Cory targets.
- Validation layers were unavailable in the review environment, so GPU tests ran
  without Khronos validation despite requesting it. CI intended to validate Vulkan
  behavior should install and verify the validation layer explicitly.
- `./cbt test` builds the default target before running a focused regex, which built
  all examples during this review. A test-target-only build path would improve the
  iteration goal in ADR-07.

## Recommended next sequence

1. Add TypeScript type-check/smoke coverage and secure path validation for the Pi
   tools.
2. Implement `./cbt visual-test --json` and `cory_visual_test`.
3. Add a deterministic shader triangle test and framegraph-on-failure artifact.
4. Promote readback/encoding to engine-level capture and wire one headless example.
5. Replace or supplement BMP artifacts with PNG, then add CI policy/platform
   baselines for non-trivial visual tests.
