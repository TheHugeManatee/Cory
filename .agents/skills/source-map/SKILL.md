---
name: source-map
description: Load a condensed repository source map for Cory. Use when you need fast orientation to repo structure, subsystem entry points, standard tools, examples, tests, or what files to read next before editing.
---

# Source Map

Use this skill when you need to orient quickly in the Cory repository without broad grepping.

## Goal

Provide a compact, task-routed map of the codebase so the agent can load only the most relevant context.

## Load Order

0. Important: The `references` folder is located in `.agents/skills/source-map/` (next to this skill file).
1. Read `references/index.md` for the top-level map.
2. Read `references/routes.md` to route the task to the smallest relevant subsystem docs.
3. Read only the matching files under `references/modules/`.
4. If the task is to create or refresh the source map itself, read `references/maintenance.md` before editing map files.
5. When writing or refreshing a module page, use `references/module-template.md` as the default structure.
6. Use the referenced project skills when the task enters a specialized area:
   - `cbt` for configure/build/test/run/analyze/fmt
   - `kdgpu` for KDGpu or KDGpuUtils API work
   - `slang` for shader authoring or fixes
   - `cory-visual-testing` for visual test creation/debugging/baseline work

## Usage Rules

- Prefer the smallest set of files needed for the task.
- Treat this skill as a router, not a replacement for reading source files.
- When a module page names exact entry points, read those files before making changes.
- If the map is missing or stale, update the relevant module page after finishing the task.
- When updating the source map, follow `references/maintenance.md` and keep summaries evidence-backed, path-heavy, and short.
- When creating or refreshing module pages, use `references/module-template.md` unless there is a strong reason to deviate.
- Do not perform a full repo-source-map refresh by personally reading every module in one context window.
- For any repo sweep or broad refresh-from-scratch task, act as coordinator: inspect only top-level structure yourself, then delegate module summaries to sub-agents and merge the results.
- Prefer one sub-agent per module or per tightly related module pair when the refresh spans multiple subsystems.

## Maintenance Targets

Keep these files current when the repository structure changes:

- `references/index.md`
- `references/routes.md`
- `references/maintenance.md`
- affected files in `references/modules/`

## Reference Files

- Top-level index: `references/index.md`
- Task router: `references/routes.md`
- Repo overview: `references/repo-overview.md`
- Maintenance guide: `references/maintenance.md`
- Module template: `references/module-template.md`
- Subsystem maps: `references/modules/`
