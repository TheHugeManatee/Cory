# Cory Build Tool

The Cory build tool (`cbt`) is a small, in-tree CLI that wraps the project's CMake/Conan workflow and exposes a
deterministic, agent-friendly surface for common developer tasks: configure, build, test, run, analyze, format and
lint. The tool is intentionally conservative: prefer `cbt` over calling CMake, Conan, Ninja, or CTest directly so
automation and agents have a single canonical entrypoint.

This document is written as an agent "skill" — it explains context, when and how to use `cbt`, common flows, and
the platform-relevant differences an agent should care about.

## Purpose & Context (Agent skill)

`cbt` exists to make the Cory repository trivially automatable and reproducible. For agents, it provides:

- A single, documented entrypoint for all build and test tasks.
- Deterministic default paths and output (so automation can locate build artifacts and compile databases reliably).
- Machine-friendly flags such as `--json`, `--dry-run`, and `--no-color` for programmatic consumption.

When to use `cbt`:

- Always prefer `cbt` for configure/build/run/test/lint/format/doctor/status workflows.
- Use `cbt doctor` before other operations to detect missing prerequisites and get exact fix commands.
- Use `cbt configure` once per profile/build-dir, then use `cbt build`, `cbt test`, `cbt run`, etc. against the stored
  configuration.

## Quick Reference

Run `cbt --help` or `cbt <command> --help` for detailed per-command options. The commands most used by agents are:

- `doctor` — validate environment and print exact fix commands (run this first).
- `configure` — create a build directory, venv, run Conan and CMake, and write the persistent config.
- `build` — build the project (supports `--target` and `--jobs`).
- `test` / `tests` — discover and run tests (supports regex selection; builds tests if necessary).
- `run` — run an executable from the build dir (builds if missing unless `--no-build`).
- `fmt` / `lint` — format and lint changed files by default; accept explicit paths.
- `which` / `status` / `env` — inspect tool paths, current config, and runtime environment.

Machine-friendly flags to prefer in automation:

- `--json` — where supported, for parsing output.
- `--dry-run` — print commands that would run without executing them.
- `--no-color` — remove ANSI color codes for log collectors.

### Global UX rules (agent guidance)

- Always run `cbt doctor` as a preflight check. If `doctor` reports missing prerequisites, use the provided one-line fixes.
- `cbt` prints the underlying commands it runs (unless `--quiet`); prefer `--dry-run` to capture those commands without side-effects.
- `cbt run` is allowed to build a target if missing; it will never delete user data. Use `--no-build` to avoid implicit builds.
- `cbt clean` requires an explicit confirmation flag to avoid accidental deletion; automation should never call it without explicit intent.
- Exit codes are stable and should be used to classify failures: `2` = prerequisite missing, `3` = config missing/invalid, `4` = underlying tool failure.

### Implementation notes

 - needs to be able to run on Windows and Linux
 - needs to be able to call conan and cmake with appropriate arguments
 - needs to invoke binaries / tests as necessary
 - as part of the tools above, needs to be able set up the environments by e.g. activating virtual envs, conanbuildenv, conanrunenv, MSVC command line setup etc. before invoking the underlying commands
 - linux: uses clang-22 and clang-tidy for static analysis
 - windows: uses MSVC 2022

## Revised implementation sketch (Python + Click, minimal config)

### Design goals (short)

- Minimal external assumptions: `cbt` prefers to create its own venv under the build root and install required Python tools there.
- Deterministic paths: repo-root is resolved relative to the installed `cbt` package; build roots have platform-safe defaults.
- Configuration is persisted in the build dir (`.cbt/config.toml`); other commands rely on that file and will refuse to proceed if it's missing.
- `cbt` defers SDK discovery to CMake/CMake scripts (e.g. Vulkan is found via `find_package(Vulkan ...)`). Use `cbt configure --vulkan-sdk` only when needed.

Additional agent-focused goals:
- “Do what I mean” defaults for an agent:
  - `cbt build` uses the last configured profile if possible.
  - `cbt test <regex>` builds tests (if needed) then runs exactly that test/regex.
  - `cbt fmt` and `cbt lint` default to the changed files in git (if available).
  - `cbt run <target>` resolves `bin/<target>` and warns if the target doesn’t exist.
- Keep the number of “modes” small: prefer one canonical path per task.
- Prefer robust detection over assumptions (e.g. find `clang-tidy-22` if present, fall back to `clang-tidy`).

### Build-root & venv (agent-visible differences)

The key cross-platform differences agents should be aware of (high level, no implementation detail):

- Default build-root:
  - Linux: `~/cory-work/` (shared location outside the repo; good for fast native build directories under WSL/Linux).
  - Windows: `<repo>/build` (inside the repository tree).
- Virtual environment location:
  - Linux: `<build_root>/.venv`
  - Windows: `<repo>/build/.venv`
  Agents should generally run `cbt setup` or `cbt configure` to ensure the venv exists; do not assume Python packages are globally available.
- Developer toolchain differences to consider:
  - Linux: CI/agents expect Clang (clang-22 preferred) and clang-tidy/clang-format variants.
  - Windows: MSVC toolchain must be available when `cbt` invokes builds that require it; `cbt` can activate the MSVC developer environment when needed but agents should prefer running `cbt doctor` to surface missing MSVC components.

These are practical concerns for invocation and environment — not implementation details of the `cbt` package.
```
tools/cbt/
  cbt/__init__.py
  cbt/__main__.py
  cbt/cli.py
  cbt/config.py
  cbt/paths.py
  cbt/conan.py
  cbt/cmake.py
  cbt/run.py
  cbt/compile.py
  cbt/analyze.py
  cbt/util.py
  pyproject.toml
```

### Where agents should start: recommended workflow

1. `cbt doctor --json` — check environment and receive actionable one-line fixes.
2. `cbt configure --profile <name> --build-type <Debug|Release>` — sets up venv, runs conan and cmake and writes `.cbt/config.toml`.
3. `cbt build --jobs <N> [--target <target>]` — build artifacts.
4. `cbt test '<regex>'` or `cbt run <target> -- <args>` — execute tests or targets (use `--no-build` to avoid implicit builds).
5. `cbt fmt` and `cbt lint` — operate on changed files by default; pass explicit paths to override.

Notes for agents:

- Persist or cache the build-dir path returned by `cbt configure` (it writes `.cbt/config.toml` into the build dir). Subsequent commands reuse the config.
- Use `--dry-run` when constructing commands to be executed by other systems so you can extract exact invocation lines.
- Prefer `--json` output where available to avoid brittle text parsing.

### Reproducibility & safety

- `cbt` prints a "Reproduce manually:" block with exact commands when failures occur — copy those commands to reproduce locally.
- Avoid `cbt clean` in automation unless explicitly intended; it requires confirmations to prevent accidental deletion.
- `cbt configure` is the only command that writes the config file; other commands will refuse to guess missing settings.

### Examples — one-liners agents should memorize

Configure and build (idempotent):

```
cbt doctor --json
cbt configure --profile codex --build-type Debug
cbt build --jobs 16
```

Run a single test by regex (builds tests if required):

```
cbt test "unittests\\..*SlangCompiler.*"
```

Format and lint changed files:

```
cbt fmt
cbt lint
```

Get the environment required to reproduce a run (useful for launching debuggers or external runners):

```
cbt env --profile codex
```

### Conan + CMake usage (canonical)
Conan:
- `conan profile detect --force` (if needed)
- `conan install <source> --output-folder <build_dir> --build=missing -pr:h <host> -pr:b <build> -s build_type=<type>`

CMake:
- `cmake -S <source> -B <build_dir> -G Ninja -DCMAKE_TOOLCHAIN_FILE=<build_dir>/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=<type>`
- `cmake --build <build_dir> --config <type>`

No reliance on repo-level `CMakeUserPresets.json`.

### Vulkan / SDK discovery policy (keep it canonical)
Rely on CMake’s Vulkan discovery instead of custom tool logic:
- `find_package(Vulkan ...)` (or your CMake wrapper around it) is the single source of truth.
- `cbt` does not try to guess SDK paths, does not write `VULKAN_SDK`, and does not hardcode include/lib paths.

Why we previously considered SDK probing in `cbt`:
- Vulkan SDK is often installed to non-standard locations (e.g. `$HOME/VulkanSDK/...`) where default CMake search may not find it without hints.
- Cory also needs `slang`/shader tooling; when that comes from the SDK, users frequently end up needing an SDK hint anyway.

How to handle non-standard SDK installs without `cbt` special-casing:
- Recommended: user sets `VULKAN_SDK` in their shell (or system environment) if they installed the LunarG SDK to a custom location.
- Optional (agent-friendly): `cbt configure --vulkan-sdk <path>` persists a hint and applies it via:
  - environment when invoking CMake/CTest/targets (`VULKAN_SDK=<path>`)
  - cache variables via `--cmake-define` (e.g. `Vulkan_INCLUDE_DIR`, `Vulkan_LIBRARY`)

`cbt doctor` should be the place that spots common Vulkan issues and suggests exactly one fix path:
- “System packages only” (Ubuntu `libvulkan-dev`, `glslang-tools`, etc.).
- Or “Vulkan SDK present, but not discoverable” → show `cbt configure --vulkan-sdk ...`.

### Platform differences (what matters to agents)

- Invocation scripts: the repo includes `cbt` (POSIX shell) and `cbt.ps1` (PowerShell) launchers. Use whatever matches the host environment.
- Venv location differs by platform (see "Build-root & venv"), so absolute paths to the venv should come from `cbt status` / `.cbt/config.toml` rather than being guessed.
- On Windows, MSVC availability and the developer command-prompt are relevant; `cbt doctor` will flag missing MSVC components.
- On Linux (and WSL) prefer placing build roots on a native filesystem (`~/cory-work`) for performance; the documentation encourages this.

Agents do not need to know internal implementation details — prefer these high-level invariants when scripting or making decisions.

### Troubleshooting tips for agents

- If `cbt doctor` reports missing `conan`/`cmake`/`ninja`, run `cbt setup` (or `cbt configure`) to create/install into the build-root venv.
- If a target or test cannot be found, confirm the correct profile/build-dir with `cbt status` and that `cmake` configure completed successfully.
- For Vulkan-related failures, prefer `cbt configure --vulkan-sdk <path>` or set `VULKAN_SDK` in the environment used for invocations; `cbt doctor` will explain which option to use.

---

This document augments the command reference by providing an explicit, agent-oriented workflow and the practical
platform differences you need to script `cbt` reliably. For the implementation-level design and API sketches, keep
the original sections below as a reference.

#### `cbt configure` (one-time setup)
Responsibilities:
1) Resolve repo root → source dir.
2) Determine build root (platform default unless `--build-root` set).
3) Pick profile name (`--profile` default `codex`).
4) Create venv under build root.
5) Run Conan install.
6) Run CMake configure with Ninja.
7) Persist `.cbt/config.toml` in build dir.

Options (configure only):
- `--profile <name>` (default: `codex`)
- `--build-type <Debug|Release>` (default: `Debug`)
- `--build-root <path>` (optional)
- `--profile-host`, `--profile-build` (optional; stored in config)
- `--cc <path>` / `--cxx <path>` (optional; stored in config)
- `--cmake-define KEY=VALUE` (repeatable; stored in config)
- `--vulkan-sdk <path>` (optional; stored in config as a hint)
- `--export-compile-commands/--no-export-compile-commands` (default on; stored in config)
- `--force` (re-runs configure even if already configured)

Notes:
- Cory’s tests are registered via `catch_discover_tests(...)`, so `ctest -N` and `ctest -R <regex>` are the canonical interface for selecting tests.

#### `cbt reconfigure`
- Re-runs CMake configure using the stored config (no Conan install unless `--conan` is provided).
- Intended for toggling CMake defines or after toolchain updates.

Options:
- `--conan` (re-run conan install first)
- `--cmake-define KEY=VALUE` (repeatable; overrides stored values for this run)

#### `cbt build`
Uses stored config and runs:
- `cmake --build <build_dir> --config <type> --target <target?> -- -j<N>`

Options:
- `--target <name>`
- `--jobs <n>`
- `--profile <name>` (optional convenience selector)
- `--verbose` (pass-through to CMake/Ninja)

#### `cbt run <target>`
- Resolve executable path: `<build_dir>/bin/<target>` (or platform variant).
- Run with Conan run env if present (optional).
- (Agent convenience) default behavior may build the target first if it does not exist.

Options:
- `--working-dir`
- `--` passthrough args
- `--no-build` (skip the “build-if-missing” step)

#### `cbt targets`
- List targets using `cmake --build <build_dir> --target help` and best-effort parsing.
- Provide `--json` for machine-readable output.

#### `cbt tests`
- List tests using `ctest --test-dir <build_dir> -N`.
- Provide `--json` for machine-readable output.

#### `cbt test [<regex>]`
- Ensures tests are built (at minimum `--target tests` if it exists, else `Cory_Tests`, else `all` + warn).
- Runs exactly one selected test (or regex) using CTest:
  - `ctest --test-dir <build_dir> -R <regex> --output-on-failure`

Options:
- `--profile <name>`
- `--label <label-regex>` (optional)
- `--repeat <n>` / `--until-fail <n>` (optional)
- `--verbose` (maps to `ctest -V`)
- `--gdb` / `--lldb` (best-effort wrapper for a single test run)
- `--timeout <sec>` (maps to `ctest --timeout`)

#### `cbt compile <sources...>`
- Require `compile_commands.json` in build dir.
- Use compiler flags from compile database.
- Default to “compile only” (no link) and print the exact compiler command.

Design note (agent ergonomics):
- Prefer “compiles this TU like the project would” over inventing ad-hoc include paths.
- If multiple entries exist for a file in the compile DB, pick the one whose command contains the file path exactly (or prompt to disambiguate with `--target`).

#### `cbt analyze <sources...>`
- Require `compile_commands.json` in build dir.
- Run `clang-tidy` with flags from compile database.
- Provide `--fix` (if supported) and `--checks` pass-through for targeted runs.

#### `cbt fmt [paths...]`
- Run `clang-format -i` for:
  - the provided files/dirs, OR
  - (default) changed files relative to `HEAD` (best-effort: `git diff --name-only`).
- Should ignore generated code and build dirs (`build/`, `<build_root>/...`).
- Provide `--check` to report diffs without modifying files.

#### `cbt lint [paths...]`
- Run `clang-tidy` for:
  - the provided files, OR
  - (default) changed C/C++ files relative to `HEAD`.
- Uses the compile DB (`-p <build_dir>`) so results match project configuration.
- Provide a summary at the end (“X warnings, Y errors”), plus an option to print full output.
- Provide `--checks` and `--warnings-as-errors` pass-throughs for targeted runs.

#### `cbt clean`
- Removes build artifacts for a profile (or the whole build root).
- Must require explicit confirmation:
  - `cbt clean --profile codex --yes`
  - or `cbt clean --confirm codex` (guards against “wrong profile” deletions)

#### `cbt cache`
- Safe cache management commands (no destructive default):
  - `cbt cache show` (prints locations and sizes)
  - `cbt cache prune` (removes unused build dirs)
  - `cbt cache conan` (clears Conan cache with confirmation)

#### `cbt doctor`
Primary responsibility: stop agents from wasting time on environmental issues.

Checks (best-effort, platform-aware):
- Python/venv usable; `conan`, `cmake`, `ninja` present (or installable into the venv).
- Compiler availability (Linux: `clang++` and optionally `clang++-22`; Windows: MSVC 2022 via VS tooling).
- Vulkan basics (at least one of: system Vulkan dev packages or a Vulkan SDK hint) and that required shader tools are discoverable (`glslangValidator`).
- Build dir health (config file exists, toolchain file exists, compile DB exists after configure).

Output:
- A short “OK / Missing / Fix” table.
- One-liner commands to fix each missing requirement.

#### `cbt status`
- Prints the resolved config:
  - repo root, build root, build dir, build type, compiler paths
  - Conan profiles used
  - whether `compile_commands.json` exists

#### `cbt which <tool>`
- Print the resolved path to a tool, falling back to `PATH` if not in config.

#### `cbt env`
- Print the exact environment that would be used for running a target or test:
  - venv paths, `VULKAN_SDK` (if set), `PATH`, and compiler overrides.

### Preset policy
- Do not use `CMakeUserPresets.json` for automation.
- If a user wants presets, they can add them, but `cbt` should not depend on them.

### Implementation notes (module detail)

`cbt/config.py`
- `load_config(build_dir)` reads `.cbt/config.toml`.
- `write_config(build_dir, config)` persists config.
- `require_config()` for build/run/analyze.

`cbt/paths.py`
- `repo_root()`, `default_build_root()`, `build_dir_for_profile(profile)`.

`cbt/conan.py`
- `ensure_profile_detected()`
- `install(build_dir, source_dir, profile_host, profile_build, build_type)`

`cbt/cmake.py`
- `configure(build_dir, source_dir, build_type, toolchain_file, vulkan_sdk)`
- `build(build_dir, build_type, target, jobs)`

`cbt/util.py`
- `run(cmd, cwd=None, env=None)`
- (no Vulkan SDK probing; keep discovery inside CMake)

Additional modules likely needed for agent-friendly commands:

`cbt/ctest.py`
- `list_tests(build_dir)`
- `run_tests(build_dir, regex, extra_args)`

`cbt/git.py`
- `changed_files(repo_root)` (best-effort; if git unavailable, require explicit paths)

`cbt/format.py`
- `format_files(clang_format, files)`

`cbt/tidy.py`
- `tidy_files(clang_tidy, build_dir, files)`

`cbt/tools.py`
- `resolve_tool(name, config, fallback_names)` (e.g. `clang-tidy-22` → `clang-tidy`)

### Example flow
```
cbt configure --profile codex --build-type Debug
cbt build --jobs 16
cbt run SceneGraphDemo -- --help
```

### Agent-oriented “one-liners”

Common tasks an agent should be able to do without remembering details:

```
# Validate env and show exactly what to fix.
cbt doctor

# Configure once (idempotent if already configured).
cbt configure --profile codex --build-type Debug

# Build a specific target.
cbt build --target Cory_Tests

# Run a single test (Catch2 tests discovered into CTest).
cbt test "unittests\\..*SlangCompiler.*"

# Compile-check one translation unit using the project’s compile flags.
cbt compile src/Cory/SomeFile.cpp

# Lint only changed files.
cbt lint

# Format only changed files.
cbt fmt

# List tests and run one
cbt tests
cbt test "unittests\\..*FrameGraph.*"

# Show resolved tool paths
cbt which cmake
cbt which clang-tidy
```
