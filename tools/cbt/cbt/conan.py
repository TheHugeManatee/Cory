from __future__ import annotations

from pathlib import Path

from .util import run


def ensure_profile_detected(conan: str, quiet: bool) -> None:
    run([conan, "profile", "detect", "--force"], quiet=quiet)


def install(
    conan: str,
    build_dir: Path,
    source_dir: Path,
    profile_host: str,
    profile_build: str,
    build_type: str,
    quiet: bool,
) -> None:
    cmd = [
        conan,
        "install",
        str(source_dir),
        "--output-folder",
        str(build_dir),
        "--build=missing",
        "-s",
        f"build_type={build_type}",
        "-pr:h",
        profile_host,
        "-pr:b",
        profile_build,
    ]
    run(cmd, quiet=quiet)
