from __future__ import annotations

import re
from pathlib import Path

from .util import run


def list_tests(ctest: str, build_dir: Path, env: dict[str, str] | None, quiet: bool) -> list[str]:
    result = run([ctest, "--test-dir", str(build_dir), "-N"], env=env, quiet=quiet)
    tests: list[str] = []
    for line in result.stdout.splitlines():
        match = re.match(r"^\s*Test\s+#\d+:\s+(.*)$", line)
        if match:
            tests.append(match.group(1).strip())
    return tests


def run_tests(
    ctest: str,
    build_dir: Path,
    regex: str,
    label: str | None,
    repeat: int | None,
    until_fail: int | None,
    verbose: bool,
    gdb: bool,
    lldb: bool,
    timeout: int | None,
    env: dict[str, str] | None,
    quiet: bool,
) -> None:
    cmd = [ctest, "--test-dir", str(build_dir), "-R", regex, "--output-on-failure", "-j"]
    if label:
        cmd += ["-L", label]
    if repeat:
        cmd += ["--repeat", f"until-fail:{repeat}"]
    if until_fail:
        cmd += ["--repeat", f"until-fail:{until_fail}"]
    if verbose:
        cmd.append("-V")
    if gdb:
        cmd.append("--gdb")
    if lldb:
        cmd.append("--lldb")
    if timeout:
        cmd += ["--timeout", str(timeout)]
    run(cmd, env=env, quiet=quiet)
