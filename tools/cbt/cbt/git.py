from __future__ import annotations

from pathlib import Path

from .util import run


def changed_files(repo_root: Path, quiet: bool) -> list[str]:
    try:
        result = run(["git", "diff", "--name-only", "HEAD"], cwd=repo_root, quiet=quiet)
    except Exception:
        return []
    return [line.strip() for line in result.stdout.splitlines() if line.strip()]
