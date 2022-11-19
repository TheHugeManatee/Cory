# Performance Observations/Notes

This is just a set of random notes/observations regarding performance.

## Setup

Unless indicated otherwise, these perf measurements were measured on the following system:

- Intel Core i9-10900 @ 2.80GHz
- NVidia RTX 3080 10GB

## Dynamic Rendering

Using `VK_KHR_dynamic_rendering` seems to have a noticeable negative effect.

Setup: 1024x1024 viewport, 200 cubes, Release build.
Performance (5 second average after 3 second warmup):

| Renderpass | Transition          | Stencil Store | Timing (ms) |
|------------|---------------------|---------------|-------------|
| Explicit   | Implicit            | DontCare      | 0.778       |
| Dynamic    | Explicit, Once      | DontCare      | 0.856       |
| Dynamic    | Explicit, per Frame | DontCare      | 0.882       |
| Dynamic    | Explicit, per Frame | Store         | 0.854       |

Pipeline Creation went from **~90ms** to **~70ms**.

### Possible reasons

- Something around the resolve is f'ed (we're still using an explicit render pass object for ImGui)
- I ported some flags incorrectly
- Dynamic rendering is just slower