---
name: slang
description: Author, review, and refactor Slang shader code for Vulkan-focused rendering projects. Use when Codex needs to create new `.slang` shaders, migrate HLSL-style code to idiomatic Slang, fix Slang compiler errors, optimize shader structure/performance, or enforce consistent usage of modules, generics, interfaces, and parameter blocks.
---

# Slang Code

Build correct, maintainable Slang shaders quickly, then validate with the local toolchain.

## Workflow

1. Confirm intent and constraints
- Identify shader stage(s), target profile/backend, resource model, and performance goals.
- If revising existing code, preserve external contracts first: entry point names, semantics, descriptor expectations, and output formats.

2. Map current shader shape
- Read nearby modules and shared types before editing.
- Prefer minimal, local changes unless the request explicitly asks for architectural refactoring.

3. Apply idiomatic Slang patterns
- Prefer `import` modules over preprocessor includes.
- Prefer `let` for immutable intermediate values and `var` only when mutation is required.
- Use `ParameterBlock<T>` and typed resources to keep bindings explicit and reflectable.
- Use interfaces/generics for extensibility instead of macro-driven variants.
- Keep shader entry points thin; move reusable logic into helper functions/types.

4. Compile and iterate
- Use `./cbt slang <path-to-shader>` from repo root after meaningful edits.
- Fix diagnostics from first root cause outward (syntax/type errors before follow-on stage or linkage errors).
- Re-run compile until clean or until blocked by missing external context.

5. Report with engineering focus
- Explain what changed, why it is safe, and where risk remains.
- Call out unresolved assumptions (binding layout, numeric precision, target capability limits).

## Project-Specific Guidance
- A general shader library for basic modules is available in `data/shaders/` and modules from there can be imported as needed. 
- Check if helpers are available there, especially for math, lighting, and common structures. If not, consider contributing useful helpers back to the library.
- The project specifically target spir-v / vulkan. This project does not use uniform/constant buffers, but rather pushes a pointer to a parameter block struct as a push constant.

## Revision Checklists

### Safe refactor checklist
- Preserve entry point signatures and semantics unless explicitly requested to change.
- Preserve resource register/set-space intent unless the task includes rebinding.
- Keep numerical behavior stable; note any intentional approximation/perf tradeoffs.

### New shader checklist
- Define stage attributes and thread-group sizes explicitly.
- Keep parameter structs cohesive; avoid scattered global state.
- Add clear names for coordinate spaces and units.
- Validate with `./cbt slang` before handing off.

## Resource Map

- Language primer: `references/slang-language-primer.md`
  - Load when you need syntax/feature refreshers (modules, generics, interfaces, parameter blocks).
- Practical patterns: `references/slang-best-practices.md`
  - Load when drafting/reviewing production-ready shader structure, performance hygiene, and fix patterns.
