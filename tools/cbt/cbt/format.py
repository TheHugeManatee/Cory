from __future__ import annotations

from pathlib import Path

from .util import run


def format_files(clang_format: str, files: list[Path], check: bool, quiet: bool) -> None:
    if not files:
        return
    cmd = [clang_format]
    if check:
        cmd += ["--dry-run", "--Werror"]
    else:
        cmd += ["-i"]
    cmd += [str(f) for f in files]
    run(cmd, quiet=quiet)
