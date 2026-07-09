---
name: cory-visual-testing
description: Create, run, debug, and update Cory TestCanvas-based visual regression tests and baselines. Use when working on headless GPU rendering tests, image comparisons, visual artifacts, reviewer UI tests, baseline updates, or driver-dependent render output in Cory.
---

# Cory Visual Testing

Use this skill for Cory visual tests built on `Cory::testing::TestCanvas`, `compareToReference()`, and `requireMatchesReference()`.

## Workflow

- Start with a focused Catch2 test tagged `[visual]`.
- Render one frame with `TestCanvas`.
- Return the final color texture from the callback.
- Compare the result against a stored BMP baseline.
- Inspect `actual.bmp`, `baseline.bmp`, `diff.bmp`, `metrics.json`, and `request.json` when a comparison fails.

## Test Shape

- Prefer tiny, deterministic smoke tests first.
- Keep the render path fixed: size, colors, geometry, and inputs should not depend on time, randomness, or host-specific state.
- Prefer one visual behavior per test case.
- Use `ImageCompareOptions` only when the renderer is expected to vary slightly and exact matching is too strict.
- If rendered text would make the golden platform-specific, keep the visible label stable and move host-specific details to hover-only metadata.

## When To Use The Reference

Read `references/visual-testing.md` for:

- the canonical `TestCanvas` test template,
- how the artifact and baseline paths are named,
- how to interpret `CORY_VISUAL_RESULT`,
- baseline update policy,
- and the main troubleshooting checklist.

## Practical Rule

If a visual test fails, inspect the image artifacts before changing the test or the baseline. Update the golden only after you have confirmed the new render is intentional.
