# Cory Build Tool

The Cory build tool (`cbt`) is a platform-agnostic script that can configure, build, test, and run the Cory project.

It provides a wrapper around a CMake/Conan build system.

## Why this exists (agent-first)

This tool is primarily for **AI agents** (and humans) as a simple entry point for common tasks on this codebase:

- Single-line commands for common workflows (configure/build/run/test/lint/format).
- Minimal “tribal knowledge” required (no “remember to source X before running Y”).
- Clear, deterministic output so an agent can recover quickly from failures.

## Usage

`cbt --help` - Show help
`cbt <tool> --help` - Show help for a specific tool as below
`cbt doctor` - Validate prerequisites and print actionable fixes
`cbt status` - Print the active config and resolved paths/tools
`cbt configure [options]` - Configure the build system
`cbt reconfigure [options]` - Re-run CMake configure in an existing build dir
`cbt build [options]` - Build the project
`cbt run [options] <target>` - Run a built target
`cbt targets [options]` - List CMake targets (best-effort)
`cbt tests [options]` - List CTest tests
`cbt test [options] <regex?>` - Build tests and run a single test (or regex)
`cbt compile <source files> [options]` - Compile source files using the same settings as the project
`cbt analyze <source files> [options]` - Run static analysis on source files using the same settings as the project
`cbt fmt [paths...]` - Run clang-format on files/dirs (defaults to changed files)
`cbt lint [paths...]` - Run clang-tidy on files (defaults to changed files)
`cbt clean [options]` - Remove build artifacts (requires explicit confirmation)
`cbt cache [options]` - Manage or clear dependency/build caches (safe subcommands only)
`cbt which <tool>` - Print resolved tool path (cmake/conan/ninja/clang/clang-tidy/clang-format)
`cbt env` - Print environment needed to reproduce a run

### Global UX rules (to keep agents unconfused)

- No hidden requirements: if something is missing, fail with a short checklist and the exact command to fix it.
- Print the underlying commands being run (Conan/CMake/CTest/clang-tidy/clang-format) unless `--quiet`.
- Prefer “do the safe thing” defaults:
  - `cbt run <target>` may build the target first, but should never delete anything.
  - `cbt clean` must require `--yes` (or `--confirm <profile>`) to avoid accidental deletion.
- Deterministic paths: default build locations never depend on the current working directory.
- Make it easy to reproduce: on failure, print a “Reproduce manually:” block with the exact command line.
- Friendly to automation:
  - `--json` optional for machine-readable status/doctor output.
  - `--dry-run` prints what would run without changing the system.
  - `--no-color` for log collectors.
- Predictable exit codes (automation-friendly):
  - `0` success
  - `2` missing prereq (doctor should pass before continuing)
  - `3` config missing or invalid
  - `4` underlying tool failed (cmake/conan/ctest/etc.)

### Implementation notes

 - needs to be able to run on Windows and Linux
 - needs to be able to call conan and cmake with appropriate arguments
 - needs to invoke binaries / tests as necessary
 - as part of the tools above, needs to be able set up the environments by e.g. activating virtual envs, conanbuildenv, conanrunenv, MSVC command line setup etc. before invoking the underlying commands
 - linux: uses clang-22 and clang-tidy for static analysis
 - windows: uses MSVC 2022

## Revised implementation sketch (Python + Click, minimal config)

### Design goals (per constraints)
- No environment variables required for normal use.
- Source dir is fixed relative to the tool itself (repo root).
- Build root defaults:
  - Linux: `$HOME/cory-work`
  - Windows: `<repo>/build`
- Build dir:
  - Linux: `<build_root>/<profile>`
  - Windows: `<repo>/build/<profile>`
- Generator: always Ninja.
- Configuration is stored in the build dir. Subsequent commands reuse it.
- Vulkan is discovered by CMake (FindVulkan). `cbt` does not probe SDK locations.
- Conan home is not configurable.
- Virtual env lives inside the build root (no separate venv path).

Additional agent-focused goals:
- “Do what I mean” defaults for an agent:
  - `cbt build` uses the last configured profile if possible.
  - `cbt test <regex>` builds tests (if needed) then runs exactly that test/regex.
  - `cbt fmt` and `cbt lint` default to the changed files in git (if available).
  - `cbt run <target>` resolves `bin/<target>` and warns if the target doesn’t exist.
- Keep the number of “modes” small: prefer one canonical path per task.
- Prefer robust detection over assumptions (e.g. find `clang-tidy-22` if present, fall back to `clang-tidy`).

### Repository layout (suggested)
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

### Source dir resolution (fixed)
- `source_dir` is always resolved relative to the `cbt` package location:
  - `tools/cbt/cbt/__init__.py` → repo root is `../../..`
  - Keep this canonical: no CLI option to override.

Example helper:
```python
from pathlib import Path

def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]
```

### Build root resolution (platform defaults)
- Linux: `$HOME/cory-work`
- Windows: `<repo>/build`
- CLI option `--build-root` allowed on `configure` only (and stored in config).

Build directory convention:
- Linux: `<build_root>/<profile>` (e.g. `~/cory-work/codex`)
- Windows: `<repo>/build/<profile>` (e.g. `C:\repo\Cory\build\codex`)

### Persistent config (build-dir local)
Store a config file in `<build_dir>/.cbt/config.toml`:
```toml
[cbt]
profile = "codex"
build_type = "Debug"
last_used_utc = "<iso8601>"

[paths]
source_dir = "/path/to/repo"
build_dir = "/path/to/build_root/codex"
build_root = "/path/to/build_root"
venv_dir = "/path/to/build_root/.venv"

[conan]
profile_host = "codex-clang"
profile_build = "default"
build_missing = true

[cmake]
generator = "Ninja"
toolchain_file = "/path/to/build_root/codex/conan_toolchain.cmake"
export_compile_commands = true
defines = { "CMAKE_EXPORT_COMPILE_COMMANDS" = "ON" }

[tools]
cmake = "/path/to/venv/bin/cmake"
conan = "/path/to/venv/bin/conan"
ninja = "/usr/bin/ninja"
cc = "/usr/bin/clang"
cxx = "/usr/bin/clang++"
clang_tidy = "/usr/bin/clang-tidy"
clang_format = "/usr/bin/clang-format"
ctest = "/path/to/venv/bin/ctest"

[vulkan]
# Optional “escape hatch” if CMake can't find Vulkan/Slang without hints.
sdk = "/home/user/VulkanSDK/<version>/x86_64"
cmake_hints = { "Vulkan_INCLUDE_DIR" = "/home/user/VulkanSDK/<version>/x86_64/include",
                "Vulkan_LIBRARY" = "/home/user/VulkanSDK/<version>/x86_64/lib/libvulkan.so" }
```

Rules:
- Only `configure` writes this file.
- All other commands require this file and refuse to guess build settings.
- For agent ergonomics, commands may accept `--profile` and locate `<build_root>/<profile>/.cbt/config.toml` (or print a “run configure” error if missing).

Optional convenience:
- Store “last used profile” in `<build_root>/.cbt/last_profile` so `cbt build` can work without arguments after the first successful configure.
- Store “last used build dir” in `<build_root>/.cbt/last_build_dir` so `cbt status` can work without arguments.

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

### Virtual env handling
- Venv is created in `<build_root>/.venv` (Linux) or `<repo>/build/.venv` (Windows) and shared across profiles.
- `configure` ensures venv exists and installs `conan`, `cmake`, `ninja` (and optionally `click`, `toml`) if needed.
- Commands use the venv’s python/conan without activation:
  - `<venv>/bin/conan` or `<venv>\Scripts\conan.exe`

### CLI structure (Click)
Commands:
- `cbt configure`
- `cbt reconfigure`
- `cbt build`
- `cbt run <target>`
- `cbt targets`
- `cbt tests`
- `cbt test`
- `cbt compile <sources...>`
- `cbt analyze <sources...>`
- `cbt fmt [paths...]`
- `cbt lint [paths...]`
- `cbt clean`
- `cbt cache`
- `cbt doctor`
- `cbt status`
- `cbt which`
- `cbt env`

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
