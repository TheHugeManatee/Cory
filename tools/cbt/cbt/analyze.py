from __future__ import annotations

from pathlib import Path

from .tidy import tidy_files


def analyze_files(
    clang_tidy: str,
    build_dir: Path,
    files: list[Path],
    checks: str | None,
    fix: bool,
    quiet: bool,
) -> None:
    tidy_files(
        clang_tidy=clang_tidy,
        build_dir=build_dir,
        files=files,
        checks=checks,
        warnings_as_errors=None,
        fix=fix,
        quiet=quiet,
    )
