---
name: cbt
description: Operate Cory's build/test/run workflow through the `./cbt` command only. Use when Codex needs to configure builds, compile targets, run tests, run demos, format/lint/analyze code, inspect toolchain status, or troubleshoot environment/build issues in this repository.
---

# CBT

Use `./cbt` as the single entrypoint for project tasks. Avoid direct `cmake`, `ctest`, or binary execution unless explicitly requested.

## Core Workflow

1. Start each new session with a readiness check
- Run `./cbt start` from repo root.
- If it fails, stop and report the failure instead of attempting ad-hoc environment fixes.

2. Choose the smallest command sequence for the task
- Environment/setup issue: `./cbt doctor`, then `./cbt configure` or `./cbt reconfigure`.
- Build only: `./cbt build [--target <name>]`.
- Test: `./cbt test <regex>` (prefer focused tests over full suites).
- Run target: `./cbt run <target> --frames <N> -- <args>` for interactive apps.
- Formatting/static checks: `./cbt fmt`, `./cbt lint`, `./cbt analyze`.

3. Respect platform/build-root constraints
- On Linux/WSL, keep build roots on native Linux storage (typically `~/cory-work/`), not the mounted source tree.
- Reuse configured profiles/build dirs unless the task explicitly needs a new one.

4. Handle failures deterministically
- Use `./cbt status`, `./cbt tests`, `./cbt targets`, and `./cbt which` to inspect state.
- Fix the first root cause first (missing toolchain/configuration before downstream command failures).

## Playbooks

### Configure + build
```bash
./cbt start
./cbt doctor
./cbt configure
./cbt build
```

### Run one test scope
```bash
./cbt test "<regex>"
```

### Run an interactive example safely
```bash
./cbt run <target> --frames 120
```

### Validate shader edits
```bash
./cbt slang <shader-path>
```

## Guardrails

- Run only the specific test or regex needed.
- Do not run interactive targets without `--frames N`.
- Prefer `./cbt` command outputs over guessing paths or tool invocations.
- Keep command usage repo-root relative.

## Resource Map

- Command digest and decision notes: `references/cbt-guide.md`
- Extended repository-level background: `CBT.md`
