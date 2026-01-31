from __future__ import annotations

from pathlib import Path

from .util import is_windows


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def default_build_root() -> Path:
    if is_windows():
        return repo_root() / "build"
    return Path.home() / "cory-work"


def build_dir_for_profile(profile: str, build_root: Path) -> Path:
    if is_windows():
        return repo_root() / "build" / profile
    return build_root / profile


def venv_dir_for_build_root(build_root: Path) -> Path:
    if is_windows():
        return repo_root() / "build" / ".venv"
    return build_root / ".venv"


def config_path(build_dir: Path) -> Path:
    return build_dir / ".cbt" / "config.toml"


def last_profile_path(build_root: Path) -> Path:
    return build_root / ".cbt" / "last_profile"


def last_build_dir_path(build_root: Path) -> Path:
    return build_root / ".cbt" / "last_build_dir"
