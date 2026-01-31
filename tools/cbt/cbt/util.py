from __future__ import annotations

import json
import os
import shlex
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable
from threading import Thread


class CbtError(Exception):
    exit_code = 1


class MissingPrereq(CbtError):
    exit_code = 2


class ConfigError(CbtError):
    exit_code = 3


class ToolError(CbtError):
    exit_code = 4


@dataclass
class RunResult:
    returncode: int
    stdout: str
    stderr: str


def print_json(data: object) -> None:
    sys.stdout.write(json.dumps(data, indent=2, sort_keys=True))
    sys.stdout.write("\n")


def is_windows() -> bool:
    return os.name == "nt"


def run(
        cmd: Iterable[str],
        cwd: Path | None = None,
        env: dict[str, str] | None = None,
        quiet: bool = False,
) -> RunResult:
    if not quiet:
        sys.stdout.write(f"$ {shlex.join(cmd)}\n")
    process = subprocess.Popen(
        list(cmd),
        cwd=str(cwd) if cwd else None,
        env=env,
        text=True,
        bufsize=1,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )

    stdout_lines: list[str] = []
    stderr_lines: list[str] = []

    class _NoopSink:
        def write(self, _):
            pass
        def flush(self):
            pass

    stdout_sink = sys.stdout if not quiet else _NoopSink()
    stderr_sink = sys.stderr if not quiet else _NoopSink()

    def _drain(pipe, sink, storage: list[str]) -> None:
        assert pipe is not None
        for line in pipe:
            storage.append(line)
            sink.write(line)
            sink.flush()
        pipe.close()

    threads = [
        Thread(target=_drain, args=(process.stdout, stdout_sink, stdout_lines), daemon=True),
        Thread(target=_drain, args=(process.stderr, stderr_sink, stderr_lines), daemon=True),
    ]
    for t in threads:
        t.start()
    returncode = process.wait()
    for t in threads:
        t.join()

    if returncode != 0:
        raise ToolError(f"Command failed: {shlex.join(cmd)}")

    return RunResult(returncode, "".join(stdout_lines), "".join(stderr_lines))


def ensure_dir(path: Path) -> None:
    path.mkdir(parents=True, exist_ok=True)


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def write_text(path: Path, content: str) -> None:
    ensure_dir(path.parent)
    path.write_text(content, encoding="utf-8")


def which(name: str) -> str | None:
    from shutil import which as _which

    return _which(name)
