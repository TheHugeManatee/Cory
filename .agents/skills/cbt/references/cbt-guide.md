# CBT Guide (Agent Digest)

This reference condenses repository guidance in `CBT.md` into practical command choices for day-to-day agent work.

## Command Selection

- `./cbt doctor` - check environment/tool prerequisites.
- `./cbt configure` - create/update build configuration (Conan + CMake setup).
- `./cbt reconfigure` - rerun CMake config using stored settings.
- `./cbt build` - build default or selected target.
- `./cbt tests` - list tests.
- `./cbt test <regex>` - run selected tests.
- `./cbt run <target> --frames <N> -- <args>` - run a target safely in automation.
- `./cbt fmt` - format source files.
- `./cbt lint` - run clang-tidy checks.
- `./cbt analyze` - broader static analysis.
- `./cbt slang <shader>` - compile/validate Slang shaders.
- `./cbt status` - show active profile/build information.
- `./cbt targets` - list build targets.
- `./cbt which <tool>` - resolve tool path used by cbt.
- `./cbt env` - print environment variables for a profile.

## Common Flows

### First-use or environment uncertainty

```bash
./cbt start
./cbt doctor
./cbt configure
./cbt build
```

### Targeted test loop

```bash
./cbt test "<regex>"
```

Use focused regexes. Avoid full-suite test runs unless explicitly requested.

### Shader iteration loop

```bash
./cbt slang <path/to/shader.slang>
```

### Interactive demo in automation/CI-like environments

```bash
./cbt run <target> --frames 120
```

## Platform Notes

- Linux/WSL: Prefer build roots on native Linux filesystem (`~/cory-work/`) for performance.
- Windows: Expect MSVC toolchain requirements; use `./cbt doctor` to identify missing components.

## Troubleshooting Heuristics

1. Run `./cbt doctor` for missing-toolchain symptoms.
2. Run `./cbt status` when command behavior differs from expectations.
3. Run `./cbt targets` / `./cbt tests` if a target/test name is not found.
4. Reconfigure before deeper debugging when build settings are stale.

## Safety Rules

- Prefer `./cbt` over direct `cmake`, `ctest`, `conan`, or manual binary invocation.
- Do not run interactive apps without `--frames N`.
- Use repo-root invocation so paths and profiles resolve consistently.
