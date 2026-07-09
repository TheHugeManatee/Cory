from __future__ import annotations

from pathlib import Path

from .util import run


def changed_files(repo_root: Path, quiet: bool) -> list[str]:
    try:
        result = run(["git", "diff", "--name-only", "HEAD"], cwd=repo_root, quiet=True)
    except Exception:
        return []
    return [line.strip() for line in result.stdout.splitlines() if line.strip()]


def changed_files_worktree(repo_root: Path, quiet: bool) -> list[str]:
    files = _gather_changed_files(
        repo_root,
        quiet,
        [
            ["git", "diff", "--name-only", "HEAD"],
            ["git", "diff", "--name-only", "--cached"],
            ["git", "ls-files", "--others", "--exclude-standard"],
        ],
    )
    return files


def changed_files_against(repo_root: Path, branch: str, quiet: bool) -> list[str]:
    try:
        # Files different between the given branch and HEAD (committed changes)
        result_base = run(["git", "diff", "--name-only", f"{branch}...HEAD"], cwd=repo_root, quiet=True)
    except Exception:
        result_base = None
    try:
        # Unstaged changes relative to HEAD
        result_unstaged = run(["git", "diff", "--name-only", "HEAD"], cwd=repo_root, quiet=True)
    except Exception:
        result_unstaged = None
    try:
        # Staged (cached) changes
        result_staged = run(["git", "diff", "--name-only", "--cached"], cwd=repo_root, quiet=True)
    except Exception:
        result_staged = None

    files = []
    for res in (result_base, result_unstaged, result_staged):
        if not res:
            continue
        for line in res.stdout.splitlines():
            line = line.strip()
            if line:
                files.append(line)
    # preserve order but unique
    seen = set()
    uniq: list[str] = []
    for f in files:
        if f not in seen:
            seen.add(f)
            uniq.append(f)
    return uniq


def changed_files_against_with_untracked(repo_root: Path, branch: str, quiet: bool) -> list[str]:
    files = changed_files_against(repo_root, branch, quiet)
    files = _unique_preserve_order(files + untracked_files(repo_root, quiet))
    return files


def untracked_files(repo_root: Path, quiet: bool) -> list[str]:
    try:
        result = run(["git", "ls-files", "--others", "--exclude-standard"], cwd=repo_root, quiet=True)
    except Exception:
        return []
    return [line.strip() for line in result.stdout.splitlines() if line.strip()]


def _gather_changed_files(repo_root: Path, quiet: bool, commands: list[list[str]]) -> list[str]:
    files: list[str] = []
    for cmd in commands:
        try:
            result = run(cmd, cwd=repo_root, quiet=True)
        except Exception:
            continue
        for line in result.stdout.splitlines():
            line = line.strip()
            if line:
                files.append(line)
    return _unique_preserve_order(files)


def _unique_preserve_order(files: list[str]) -> list[str]:
    seen = set()
    uniq: list[str] = []
    for file_name in files:
        if file_name in seen:
            continue
        seen.add(file_name)
        uniq.append(file_name)
    return uniq
