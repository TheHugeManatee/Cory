# ADR-08 Unit-Test Driven Visual Testing with TestCanvas

## Status
Accepted for incremental implementation.

Phase 1 MVP has started with a test-only `TestCanvas` harness, BMP baseline/artifact helpers, a clear-color visual smoke test, and the first hybrid visual-review protocol/tool skeleton.

## Context
Cory already has the core primitives needed for isolated GPU tests:

- `tests/TestUtils.*` provides `Cory::testing::VulkanTester`, backed by a shared headless `Context`.
- `Context::setupHeadlessDevice()` creates a device without a window/surface.
- `HeadlessFrameSource` creates offscreen color/depth/swapchain-like images and yields `FrameContext` values through `FrameGenerator`.
- `Framegraph` can import a `FrameContext`, record tasks, and submit through the frame source.
- `StandardRenderTasks::copyToTarget` can copy/resolve a produced color image into the frame-context swapchain image.
- Catch2 is the existing unit-test framework.

The desired workflow is to make visual rendering tests feel like normal unit tests:

```cpp
TEST_CASE("My render task draws a red quad", "[visual][framegraph]")
{
    Cory::testing::VulkanTester tester;
    Cory::testing::TestCanvas canvas{tester.ctx(), glm::u32vec2{100, 100}};

    auto image = canvas.render([](Cory::testing::TestFrame &frame) {
        auto cleared = Cory::StandardRenderTasks::clearAttachments(
            frame.graph.declareTask("TASK_Clear"),
            frame.color,
            frame.depth,
            Gpu::ColorClearValue{1.0f, 0.0f, 0.0f, 1.0f});
        return cleared.output().color;
    });

    CHECK_THAT(image, Cory::testing::MatchesReference("red-clear"));
}
```

Agents should then be able to create a focused test, run one `./cbt test` regex, and inspect structured artifacts: actual PNG, baseline PNG, diff PNG, and JSON metrics.

## Decision
Introduce a test-only visual harness centered around `Cory::testing::TestCanvas`. The harness will live in tests/support code initially, not in the public engine API. Engine-level reusable pieces for GPU readback and image encoding may be promoted to `src/Cory` once stable.

The workflow will be supported by:

1. A C++ `TestCanvas` helper for one-frame offscreen rendering inside Catch2 tests.
2. A C++ image readback and comparison utility.
3. A baseline/artifact directory convention.
4. A `cbt visual-test` command or equivalent `cbt test` wrapper.
5. A Pi extension tool that runs visual tests and summarizes artifacts.
6. A Pi skill documenting the workflow for agents.

## Proposed C++ API

### `TestCanvas`

Location:

- `tests/VisualTestUtils.hpp`
- `tests/VisualTestUtils.cpp`

Initial test-only API:

```cpp
namespace Cory::testing {

struct TestCanvasCreateInfo {
    glm::u32vec2 size{128, 128};
    Gpu::Format colorFormat{Gpu::Format::B8G8R8A8_UNORM};
    Gpu::SampleCountFlagBits samples{Gpu::SampleCountFlagBits::Samples1Bit};
    std::string label{"TestCanvas"};
};

struct TestFrame {
    FrameContext &frameCtx;
    Framegraph &graph;
    Framegraph::FrameContextHandles frame;

    TransientTextureHandle color;
    TransientTextureHandle depth;
    TransientTextureHandle swapchain;
};

struct ImageRgba8 {
    glm::u32vec2 size{};
    std::vector<std::byte> pixels; // RGBA8, tightly packed, top-left origin
};

class TestCanvas : NoCopy, NoMove {
  public:
    TestCanvas(Context &ctx, glm::u32vec2 size);
    TestCanvas(Context &ctx, TestCanvasCreateInfo createInfo);
    ~TestCanvas();

    Context &ctx();
    glm::u32vec2 size() const;

    // Most common path: callback declares passes and returns the image to capture.
    template <typename RenderFunc>
    ImageRgba8 render(RenderFunc &&renderFunc);

    // Lower-level path: callback fully manages framegraph outputs.
    template <typename RenderFunc>
    ImageRgba8 renderDeclared(RenderFunc &&renderFunc);

    const std::filesystem::path &lastActualPath() const;
};

} // namespace Cory::testing
```

Recommended callback shape:

```cpp
auto actual = canvas.render([](Cory::testing::TestFrame &frame) {
    // frame.color/frame.depth are imported frame-context resources.
    // Return the final color texture to be copied to the capture target.
    return myRenderTask(frame.graph.declareTask("TASK_MyPass"), frame.color, frame.depth)
        .output()
        .color;
});
```

`TestCanvas::render()` responsibilities:

1. Acquire exactly one frame from `HeadlessFrameSource::frames()`.
2. Reset/prepare a single `Framegraph` instance.
3. Import frame-context color/depth/swapchain images.
4. Construct `TestFrame` and invoke the user callback.
5. If callback returns a `TransientTextureHandle`, copy/resolve it to the imported swapchain image via `StandardRenderTasks::copyToTarget` and declare that copied target as output.
6. Record the framegraph.
7. Let `FrameGenerator::iterator` submit the frame, then wait on `frameCtx.inFlight` or `ctx.device().waitUntilIdle()`.
8. Read back the swapchain image into `ImageRgba8`.
9. Optionally write the actual PNG to the artifact directory.

Important implementation note: `FrameGenerator::iterator` submits the pending frame in `operator++()` or in its destructor. `TestCanvas` should make submission explicit by advancing/destroying the iterator in a small scope, then waiting before readback.

### Image readback

Add a readback helper. It may start in `tests/VisualTestUtils.*` and later move to `src/Cory/Renderer/FrameCapture.*`.

Proposed internal API:

```cpp
ImageRgba8 readbackTextureRgba8(Context &ctx,
                                const Texture &texture,
                                glm::u32vec2 size,
                                Gpu::Format format,
                                std::string_view label);
```

Implementation plan:

1. Create a CPU-visible readback buffer with usage `TransferDstBit` and memory usage suitable for mapping.
2. Record a one-shot command buffer on the graphics queue:
   - transition the texture from final framegraph state to transfer source if needed,
   - copy texture to buffer,
   - transition back if needed or leave at a terminal test-only state.
3. Submit with a fence and wait.
4. Map buffer and normalize/copy rows into tightly packed RGBA8.
5. Handle at least `B8G8R8A8_UNORM` and `R8G8B8A8_UNORM` initially.
6. Add assertions/errors for unsupported formats.

If KDGpu does not expose texture-to-buffer copy in the current abstraction, implement the readback helper with raw Vulkan using handles from `KDGpu::VulkanResourceManager`. Keep that Vulkan-specific code isolated in one helper.

### PNG and artifact IO

Add a tiny image IO helper:

```cpp
void writePng(const std::filesystem::path &path, const ImageRgba8 &image);
Result<ImageRgba8> readPng(const std::filesystem::path &path);
```

Implementation options:

- Use `stb_image_write`/`stb_image` in test utilities first.
- If useful outside tests, promote to `src/Cory/IO/Image.*`.

### Reference comparison

Proposed API:

```cpp
struct ImageCompareOptions {
    uint8_t perChannelTolerance{0};
    double maxMeanAbsoluteError{0.0};
    double maxMismatchRatio{0.0};
};

struct ImageCompareResult {
    bool passed{};
    glm::u32vec2 size{};
    uint64_t mismatchedPixels{};
    double mismatchRatio{};
    uint8_t maxChannelError{};
    double meanAbsoluteError{};
    std::filesystem::path baselinePath;
    std::filesystem::path actualPath;
    std::filesystem::path diffPath;
    std::filesystem::path metricsPath;
};

ImageCompareResult compareToReference(std::string_view caseName,
                                       const ImageRgba8 &actual,
                                       ImageCompareOptions options = {});
void requireMatchesReference(std::string_view caseName,
                             const ImageRgba8 &actual,
                             ImageCompareOptions options = {});
```

Comparison behavior:

- Baselines live in `tests/baselines/visual/<caseName>.png`.
- Artifacts live in `build/.../visual-artifacts/<test-name>/<caseName>/` or a path from `CORY_VISUAL_ARTIFACT_DIR`.
- If `CORY_UPDATE_VISUAL_BASELINES=1`, write/update the baseline and pass with a clear log message.
- If the baseline is missing and update mode is not enabled, fail and write the actual image as an artifact.
- Always write `actual.png` and `metrics.json` on failure.
- Write `diff.png` on failure, using a high-contrast diff visualization.
- Print stable machine-parseable lines:

```text
CORY_VISUAL_RESULT {"case":"red-clear","passed":false,"actual":"...","baseline":"...","diff":"...","metrics":"..."}
```

These lines are for the Pi tool to parse.

## Directory conventions

Recommended paths:

```text
tests/
  VisualTestUtils.hpp
  VisualTestUtils.cpp
  VisualCanvas_Test.cpp
  baselines/
    visual/
      red-clear.png
      simple-triangle.png

<build-dir>/
  visual-artifacts/
    <sanitized-test-name>/
      <case-name>/
        actual.png
        baseline.png        # copied for convenience when present
        diff.png
        metrics.json
        framegraph.html     # optional later
```

Environment variables:

- `CORY_VISUAL_BASELINE_DIR` override baseline root.
- `CORY_VISUAL_ARTIFACT_DIR` override artifact root.
- `CORY_UPDATE_VISUAL_BASELINES=1` update baselines.
- `CORY_VISUAL_ALWAYS_WRITE_ACTUAL=1` write actuals even on pass.

## Initial tests

### 1. Harness smoke: clear color

File: `tests/VisualCanvas_Test.cpp`

Test:

```cpp
TEST_CASE("TestCanvas captures clear color", "[visual][TestCanvas]")
{
    Cory::testing::VulkanTester tester;
    Cory::testing::TestCanvas canvas{tester.ctx(), glm::u32vec2{32, 32}};

    auto actual = canvas.render([](Cory::testing::TestFrame &frame) {
        auto clear = Cory::StandardRenderTasks::clearAttachments(
            frame.graph.declareTask("TASK_ClearRed"),
            frame.color,
            frame.depth,
            Gpu::ColorClearValue{1.0f, 0.0f, 0.0f, 1.0f});
        return clear.output().color;
    });

    Cory::testing::requireMatchesReference("testcanvas-clear-red", actual);
}
```

Purpose:

- Validates `HeadlessFrameSource` + framegraph + copy to swapchain + readback + PNG compare.
- No shaders required.
- Stable across drivers.

### 2. Shader smoke: fullscreen/simple triangle

Use a minimal inline Slang shader or existing simple shader to render a colored primitive. This validates pipeline/shader path and catches coordinate/viewport issues.

### 3. Framegraph artifact smoke

Optionally extend `TestCanvas` to dump the framegraph HTML on failure. This gives agents both visual and dependency-graph artifacts.

## `cbt` workflow

Keep the standard path as focused Catch2 tests:

```bash
./cbt test "unittests.TestCanvas captures clear color"
```

Add a convenience wrapper later:

```bash
./cbt visual-test "clear color"
./cbt visual-test --update-baseline "clear color"
./cbt visual-test --json "clear color"
```

Wrapper responsibilities:

1. Set `CORY_VISUAL_ARTIFACT_DIR` to a deterministic build-local path.
2. Optionally set `CORY_UPDATE_VISUAL_BASELINES=1`.
3. Run `./cbt test <regex>` with a reasonable timeout.
4. Parse `CORY_VISUAL_RESULT` lines.
5. Print a compact summary and return non-zero on failure.
6. With `--json`, emit parsed results for tools/agents.

This wrapper should not replace `./cbt test`; it should make the visual path easier and more machine-readable.

## Pi extension/tool support

Add a project-local extension under:

```text
.pi/extensions/cory-visual/index.ts
```

Register a tool such as `cory_visual_test`.

Proposed tool schema:

```ts
{
  regex?: string;                 // Catch2/CTest regex, defaults to [visual]
  updateBaseline?: boolean;
  artifactDir?: string;
  timeoutSeconds?: number;
  includeImages?: boolean;        // if supported by harness UI
}
```

Tool behavior:

1. Runs `./cbt visual-test --json <regex>` if available, otherwise falls back to `./cbt test <regex>` with visual env vars.
2. Parses `CORY_VISUAL_RESULT` JSON lines from stdout/stderr.
3. Returns concise text:
   - pass/fail count,
   - failing case names,
   - max error/MAE/mismatch ratio,
   - paths to actual/baseline/diff/metrics.
4. If Pi supports image attachments/rendering for local paths in tool output, expose `actual.png` and `diff.png` directly.
5. Cache the last visual result in extension state so a follow-up command/tool can open or re-summarize it.

Additional useful tools:

- `cory_visual_open_last` — show last actual/diff paths or open them if UI supports it.
- `cory_visual_update_baseline` — rerun last failed case with update mode after explicit user approval.
- `cory_visual_list` — list tests tagged `[visual]` by wrapping `./cbt tests --json`.

Safety rule:

- Updating baselines is a semantically significant change. The tool should ask for explicit confirmation before setting `CORY_UPDATE_VISUAL_BASELINES=1`, unless the user directly requested baseline update.

## Pi skill support

Add a skill:

```text
.agents/skills/cory-visual-testing/SKILL.md
```

Skill description:

```yaml
name: cory-visual-testing
description: Create, run, debug, and update Cory unit-test driven visual tests using TestCanvas, visual baselines, and cory_visual_test tooling. Use when rendering output needs visual validation or agent-inspectable screenshots.
```

Skill content should include:

1. Minimal `TestCanvas` test template.
2. How to choose image sizes and tolerances.
3. How to run focused tests:
   - `./cbt test "..."`
   - `./cbt visual-test "..."`
   - `cory_visual_test` tool when available.
4. Artifact paths and how to interpret metrics.
5. Baseline update policy.
6. Troubleshooting checklist:
   - missing baseline,
   - readback format unsupported,
   - validation errors,
   - frame not submitted because iterator lifetime is wrong,
   - nondeterministic shaders/time/randomness.

## Implementation steps

### Phase 1: Test-only MVP

1. Add `tests/VisualTestUtils.hpp/.cpp` to `Cory_TestLib` in `tests/CMakeLists.txt`.
2. Implement `ImageRgba8`, PNG write/read, compare metrics, artifact path helpers.
3. Implement `TestCanvas` using `HeadlessFrameSource` and one `Framegraph`.
4. Implement readback for `B8G8R8A8_UNORM` and/or `R8G8B8A8_UNORM`.
5. Add `tests/VisualCanvas_Test.cpp` clear-color smoke test.
6. Generate and commit the first baseline.
7. Run focused test:

```bash
./cbt test "unittests.TestCanvas captures clear color"
```

### Phase 2: Ergonomics and observability

1. Emit `CORY_VISUAL_RESULT` JSON lines.
2. Add optional framegraph dump on visual failure.
3. Add `CORY_VISUAL_ARTIFACT_DIR` and baseline env overrides.
4. Add `./cbt visual-test` wrapper with `--json` and `--update-baseline`.
5. Add the `cory-visual-testing` skill.

Hybrid reviewer groundwork now exists:

- `tools/visual/include/Cory/Tools/VisualReviewProtocol.hpp`
- `tools/visual/src/VisualReviewProtocol.cpp`
- `tools/visual/src/VisualDiffReviewer.cpp`
- `.pi/extensions/visual-review/index.ts`

`compareToReference()` writes request/decision paths and, when `CORY_VISUAL_INTERACTIVE=1`, launches `CORY_VISUAL_REVIEWER --request <request.json>`. Launch failure, invalid decision, or rejection fails closed. Acceptance updates the baseline. Artifact directories are namespaced by Catch test name and comparison source line to avoid unrelated failing visual tests overwriting each other. Request JSON includes Catch test name plus source file/line/function metadata captured via `Catch::getResultCapture().getCurrentTestName()` and `std::source_location`. `VisualDiffReviewer` supports `--auto-accept`/`--auto-reject` for protocol tests and has a Cory `Application`/`Window`/`ImGuiLayer` based interactive UI that displays baseline vs actual/diff BMPs with zoom plus accept/reject buttons. A project-local pi extension tool (`visual_review_request`) can inspect `request.json`, attach baseline/actual/diff images and source excerpts to the model context, and optionally accept the change by updating the baseline and writing `decision.json`. In the intended agent workflow, inspection is primary; explicit rejection is optional bookkeeping, and an unintended regression usually just leads to further code iteration.

### Phase 3: Pi tool integration

1. Add `.pi/extensions/cory-visual/index.ts`.
2. Register `cory_visual_test` and parse result lines.
3. Add baseline-update confirmation flow.
4. Add image-path rendering/opening if supported by Pi UI.
5. Document tool usage in the skill.

### Phase 4: Broader renderer coverage

1. Add a simple shader/triangle visual test.
2. Add a render-task-specific test for one reusable engine task.
3. Add a SceneGraph system-level visual smoke test only if it can be deterministic.
4. Consider CI labels/profiles for visual tests.

## Design constraints and pitfalls

- Keep MVP resolution small: 32x32 to 128x128.
- Prefer deterministic clear/geometry tests before complex demos.
- Avoid wall-clock animation; inject fixed simulation time/tick data.
- Avoid random scene generation unless seeded.
- Keep baselines in linear/sRGB-consistent formats; document the chosen color format.
- Thresholds should start strict for clear-color tests and become slightly tolerant only for shader-heavy tests.
- Do not run all visual tests by default if they are slow or driver-sensitive; tag them clearly with `[visual]`.
- Keep readback format support explicit and fail loudly for unsupported formats.
- Consider that image origin may differ after readback; normalize to top-left origin and lock it with tests.

## Open questions

- Should `TestCanvas` live permanently under `tests/`, or should the core capture/readback pieces move into `src/Cory` for examples and tools?
- Should visual baselines be stored as PNG in git, Git LFS, or generated on demand for simple cases?
- Should baseline updates be a separate command to avoid accidental golden-image churn?
- How much tolerance is acceptable for non-trivial shader tests across vendors/drivers?
- Should `TestCanvas::render()` return only `ImageRgba8`, or a richer object with image, framegraph execution info, and artifact helpers?
