# Cory Visual Testing Reference

## Canonical workflow

1. Write a small Catch2 test tagged `[visual]`.
2. Render one frame through `Cory::testing::TestCanvas`.
3. Return the final color texture from the render callback.
4. Compare the captured image with `Cory::testing::requireMatchesReference()` or inspect the full `ImageCompareResult` from `compareToReference()`.
5. If the test fails, inspect artifacts first, then decide whether the render changed intentionally or the baseline is stale.

## Minimal test template

```cpp
TEST_CASE("My visual test", "[visual][TestCanvas]")
{
    Cory::testing::VulkanTester tester;
    Cory::testing::TestCanvas canvas{tester.ctx(), glm::u32vec2{32, 32}};

    auto actual = canvas.render([](Cory::testing::TestFrame &frame) {
        auto clear = Cory::StandardRenderTasks::clearAttachments(
            frame.graph.declareTask("TASK_Clear"),
            frame.color,
            frame.depth,
            Gpu::ColorClearValue{1.0f, 0.0f, 0.0f, 1.0f});
        return clear.output().color;
    });

    Cory::testing::requireMatchesReference("my-visual-test", actual);
}
```

Use this shape when the goal is to validate a render path without involving the full app.

## What a good visual test looks like

- It is deterministic.
- It uses a fixed canvas size.
- It exercises one behavior at a time.
- It does not depend on wall-clock time.
- It avoids random input unless the seed is fixed.
- It keeps the visible image stable across platforms.

If the image must show path or source information, prefer filename-only text in the pixels and expose the full value on hover or in metadata. That keeps the golden portable.

## Running tests

Use the narrowest test regex that isolates the case you are working on.

```bash
./cbt test "unittests.TestCanvas captures clear color"
./cbt test "unittests.VisualDiffReviewer UI matches reference"
```

When visual baselines are being updated intentionally, use the repo’s update mode if available, otherwise update the baseline file only after verifying the new render is correct.

## Artifact layout

Current Cory visual tests write BMP baselines and BMP artifacts.

- Baseline: `tests/baselines/visual/<caseName>.bmp`
- Artifacts: `.../visual-artifacts/<sanitized-catch-test>/<caseName>-L<line>/`
- Common artifact files:
  - `actual.bmp`
  - `diff.bmp`
  - `metrics.json`
  - `request.json`
  - `decision.json` when interactive review is involved

The comparison helper also emits a machine-readable `CORY_VISUAL_RESULT` JSON line. Use that line to see whether the case passed and where the artifacts were written.

## Comparison metrics

The key values are:

- `mismatchedPixels`
- `mismatchRatio`
- `maxChannelError`
- `meanAbsoluteError`

Check them in that order.

- `mismatchedPixels` tells you how much of the image changed.
- `maxChannelError` tells you whether the difference is a small rounding drift or a real visual change.
- `meanAbsoluteError` helps separate broad low-amplitude drift from localized changes.

## Baseline policy

- Update a baseline only when the new render is intentional.
- Do not update a baseline just to hide a cross-platform difference.
- If a platform-specific string or layout causes the capture to differ, make the rendered pixels stable instead of baking the host path into the image.
- Keep the test strict for deterministic cases like clears and simple geometry.
- Use small tolerances only when a renderer or driver makes exact matching impractical.

Relevant environment variables:

- `CORY_VISUAL_BASELINE_DIR`
- `CORY_VISUAL_ARTIFACT_DIR`
- `CORY_UPDATE_VISUAL_BASELINES`
- `CORY_VISUAL_ALWAYS_WRITE_ACTUAL`
- `CORY_VISUAL_INTERACTIVE`

## Troubleshooting

- Missing baseline: confirm the baseline file exists and the case name matches the filename.
- Frame not captured: check the `TestCanvas` callback returns the texture you want to compare.
- Readback problems: confirm the format is one of the supported RGBA8 UNORM formats.
- Review launch failure: check the reviewer executable path and the interactive review environment variable.
- Unexpected platform diff: look for absolute paths, line wrapping, font differences, or driver-specific blending.
- Validation errors: reduce the test to a clear-color or simple geometry smoke test first.

## Current Cory example files

- `tests/VisualCanvas_Test.cpp`
- `tools/visual/tests/VisualDiffReviewerUi_Test.cpp` - this is somewhat self-referential and thus does not show the best practice, so use with caution.
- `test-support/include/Cory/Testing/VisualTestUtils.hpp`
