from __future__ import annotations

from pathlib import Path

from .util import run


def configure(
    cmake: str,
    build_dir: Path,
    source_dir: Path,
    build_type: str,
    toolchain_file: Path,
    defines: dict[str, str],
    env: dict[str, str] | None,
    quiet: bool,
) -> None:
    cmd = [
        cmake,
        "-S",
        str(source_dir),
        "-B",
        str(build_dir),
        "-G",
        "Ninja",
        f"-DCMAKE_TOOLCHAIN_FILE={toolchain_file}",
        f"-DCMAKE_BUILD_TYPE={build_type}",
    ]
    for key, value in defines.items():
        cmd.append(f"-D{key}={value}")
    run(cmd, env=env, quiet=quiet)


def build(
    cmake: str,
    build_dir: Path,
    build_type: str,
    target: str | None,
    jobs: int | None,
    verbose: bool,
    env: dict[str, str] | None,
    quiet: bool,
    native_tool: str | None = None,
) -> None:
    # Prefer invoking the native build tool directly (e.g. ninja) when provided.
    # Calling the native tool avoids CMake reconfigure step that `cmake --build`
    # may trigger in some setups. Fall back to `cmake --build` when native_tool
    # is not provided.
    if native_tool:
        # For ninja, use: ninja -C <build_dir> [target] [-jN]
        cmd = [native_tool, "-C", str(build_dir)]
        if target:
            cmd.append(target)
        if jobs:
            cmd.append(f"-j{jobs}")
        # ninja's verbose flag is -v
        if verbose:
            cmd.append("-v")
        run(cmd, env=env, quiet=quiet)
        return

    cmd = [cmake, "--build", str(build_dir), "--config", build_type]
    if target:
        cmd += ["--target", target]
    extra: list[str] = []
    if jobs:
        extra.append(f"-j{jobs}")
    if verbose:
        extra.append("-v")
    if extra:
        cmd += ["--", *extra]
    run(cmd, env=env, quiet=quiet)
