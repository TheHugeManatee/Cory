# Source Map Maintenance Guide

Use this guide when creating or refreshing source-map content.

## Goal
Produce compact, evidence-backed summaries that help the next agent find the right files quickly. Keep routing value high and symbol dumps low.

## Update Modes
- **Quick patch**: a few paths, targets, or names changed.
- **Module refresh**: one directory or subsystem changed materially.
- **Repo sweep**: top-level structure, target layout, or ownership boundaries changed.

### When sub-agents are required

Use sub-agents for any task that would otherwise require the coordinator to ingest many modules or many source files at once.

Sub-agents are **required** for:
- a full refresh from scratch
- a repo sweep touching multiple modules
- adding or refreshing several module pages in one task
- any refresh where the coordinator would otherwise read broad portions of `src/Cory/`, `examples/`, and `tests/` directly

Sub-agents are **recommended** for:
- a single large module with many files
- a module that needs cross-checking against examples or tests

Sub-agents are usually **not needed** for:
- a quick patch to one module page
- a small targeted correction to paths, targets, or wording

For a repo sweep:
1. The coordinator verifies the top-level layout in `index.md`.
2. The coordinator decides the module split.
3. Launch one sub-agent per module, or per tightly related module pair if the areas are small.
4. Each sub-agent inspects only its assigned area and returns a concise module summary.
5. The coordinator merges those summaries into `references/modules/`, then updates `routes.md` and `repo-overview.md` if routing or ownership changed.

If a new major directory appears under `src/Cory/`, add a module page and route entry for it.

## Evidence Gathering Workflow
1. Inspect the directory tree.
2. Find the owning `CMakeLists.txt` or target registration file.
3. Read the main public header(s) and central implementation file(s).
4. Find one representative consumer in `examples/`, `tests/`, or a neighboring subsystem.
5. Record exact paths, exact target names, and exact symbols.

For repo-wide refreshes, the coordinator should gather only top-level evidence itself. Deeper evidence gathering belongs in the relevant sub-agent assignment.

## Summary Writing Standard
Each module page should answer:
- when should this file be loaded?
- what directory or subsystem does it cover?
- what are the most important paths?
- what concepts or services live there?
- what should be read next before editing?
- what adjacent modules or skills are relevant?
- what gotchas or invariants are worth preserving?

## Required Quality Rules
- Prefer exact relative paths over vague descriptions.
- Prefer a few durable targets/symbols over exhaustive inventories.
- State only what is supported by inspected files.
- Keep prose short and bullet-based.
- Distinguish facts from guesses.
- Do not turn the map into a full searchable index.

## Recommended Module Shape
Use `module-template.md` as the default structure unless there is a strong reason not to.

Canonical sections:

```md
# Module Name

## Load When
## Main Paths
## Important Concepts
## Read Next
## Related Skills
## Gotchas
```

## Coordinator vs Sub-Agent Responsibilities

### Coordinator
The coordinator should:
- read `index.md`, `routes.md`, this file, and `module-template.md`
- inspect only the top-level repo structure and ownership boundaries
- decide which modules need refresh
- launch sub-agents with narrow assignments
- pass `module-template.md` as the expected output shape when assigning module work
- merge, normalize, and de-duplicate the returned summaries
- update `routes.md` and `repo-overview.md` after module summaries are in place

The coordinator should **not**:
- read every module's sources directly during a repo sweep
- accumulate detailed notes for many subsystems in one context
- turn the refresh into a monolithic repo-ingestion pass

### Sub-Agent
Use a narrow assignment for one module at a time. Ask for exact paths and symbols, not exhaustive dumps.

Each sub-agent should:
- stay inside its assigned directory or subsystem
- inspect the owning target file and a few high-value sources
- inspect one nearby example, test, or consumer when useful
- use `module-template.md` as the default output format
- return a concise markdown summary in the module format
- call out uncertainty instead of guessing

Expected output:
- 1 short purpose summary
- `Load When`
- `Main Paths`
- `Important Concepts`
- `Read Next`
- `Related Skills`
- `Gotchas`

## Example Refresh Plans

### Full refresh from scratch
- Coordinator reads top-level structure and current source-map files.
- Coordinator assigns module summaries to sub-agents.
- Sub-agents return module markdown drafts.
- Coordinator merges drafts, updates routing, and trims overlap.

### Single module refresh
- If small, do it directly.
- If large or cross-cutting, use one sub-agent for the module and keep the coordinator out of the weeds.

## Example Sub-Agent Prompt Skeleton

```text
Refresh the source-map summary for the Cory module at <module-path>.
Follow the source-map maintenance guide and use references/module-template.md as the output template.
Inspect only this module, its owning target file, and at most one nearby example/test/consumer.
Return concise markdown matching the template sections.
Use exact relative paths. Include only durable pointers. Do not produce exhaustive symbol lists.
If something is unclear, state the uncertainty instead of guessing.
```

## Maintenance Principle
Optimize for future reading decisions, not for completeness. When the task is broad, preserve coordinator context by delegating module-level evidence gathering to sub-agents.
