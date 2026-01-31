from __future__ import annotations

from pathlib import Path

from .util import run


def tidy_files(
    clang_tidy: str,
    build_dir: Path,
    files: list[Path],
    checks: str | None,
    warnings_as_errors: str | None,
    fix: bool,
    quiet: bool,
) -> None:
    if not files:
        return
    cmd = [clang_tidy, "-p", str(build_dir)]
    if checks:
        cmd += [f"--checks={checks}"]
    if warnings_as_errors:
        cmd += [f"--warnings-as-errors={warnings_as_errors}"]
    if fix:
        cmd.append("--fix")
    cmd += [str(f) for f in files]
    run(cmd, quiet=quiet)
