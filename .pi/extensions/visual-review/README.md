# visual-review extension

Adds a `visual_review_request` tool for Cory visual-test approval artifacts.

## Intended workflow

1. The agent makes a code change and runs tests.
2. A visual test fails and points to a `request.json` artifact.
3. Use `visual_review_request` with `action: "inspect"` to understand the regression:
   - baseline image
   - actual image
   - diff image
   - test metadata
   - source excerpt around the failing visual assertion
4. If the change is intended, optionally call the tool again with `action: "accept"` to copy the actual image over the baseline and write `decision.json`.
5. If the change is not intended, you usually do not need to call `reject`; just continue iterating on the code until the test passes or the intended visual result is reached.

## Tool actions

- `inspect`
  - read and summarize a visual review request
  - attach images in this order:
    - IMAGE 1 = baseline
    - IMAGE 2 = actual
    - IMAGE 3 = diff
- `accept`
  - update the baseline from the actual image
  - write `decision.json`
- `reject`
  - optional bookkeeping action
  - leaves the baseline unchanged and writes `decision.json`

## Example

```text
Use visual_review_request on D:/temp/.../request.json to inspect the regression.
```

If the visual change is clearly intended:

```text
Call visual_review_request with action "accept" on the same request.
```
