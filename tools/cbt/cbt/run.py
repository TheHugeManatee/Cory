from __future__ import annotations

import os
from pathlib import Path

from .util import run


def run_target(executable: Path, args: list[str], env: dict[str, str] | None, quiet: bool) -> None:
    cmd = [str(executable)] + args
    run(cmd, env=env, quiet=quiet)
