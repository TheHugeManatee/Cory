from __future__ import annotations

import datetime as _dt
import json
import os
import shlex
import socket
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any
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


class LockError(CbtError):
    exit_code = 5


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


def utc_now_iso() -> str:
    return _dt.datetime.now(_dt.UTC).replace(microsecond=0).isoformat().replace("+00:00", "Z")


def parse_utc_iso(value: str) -> _dt.datetime:
    normalized = value.replace("Z", "+00:00")
    return _dt.datetime.fromisoformat(normalized).astimezone(_dt.UTC)


def format_age(seconds: float) -> str:
    seconds = max(int(seconds), 0)
    minutes, sec = divmod(seconds, 60)
    hours, minute = divmod(minutes, 60)
    days, hour = divmod(hours, 24)
    parts: list[str] = []
    if days:
        parts.append(f"{days}d")
    if hour:
        parts.append(f"{hour}h")
    if minute:
        parts.append(f"{minute}m")
    if sec or not parts:
        parts.append(f"{sec}s")
    return " ".join(parts)


def is_pid_alive(pid: int) -> bool | None:
    if pid <= 0:
        return None
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        return False
    except PermissionError:
        return True
    except OSError:
        return None
    return True


class CbtLock:
    def __init__(self, path: Path) -> None:
        self.path = path
        self._active = False

    def acquire(self) -> None:
        ensure_dir(self.path.parent)
        payload = {
            "pid": os.getpid(),
            "host": socket.gethostname(),
            "created_utc": utc_now_iso(),
            "cwd": os.getcwd(),
            "command": sys.argv,
        }
        try:
            fd = os.open(self.path, os.O_CREAT | os.O_EXCL | os.O_WRONLY)
        except FileExistsError as exc:
            raise LockError(self._describe_existing_lock()) from exc

        try:
            with os.fdopen(fd, "w", encoding="utf-8") as handle:
                json.dump(payload, handle, indent=2, sort_keys=True)
                handle.write("\n")
        except Exception:
            try:
                self.path.unlink(missing_ok=True)
            except Exception:
                pass
            raise
        self._active = True

    def release(self) -> None:
        if not self._active:
            return
        try:
            self.path.unlink(missing_ok=True)
        finally:
            self._active = False

    def _describe_existing_lock(self) -> str:
        lock_data: dict[str, Any] = {}
        try:
            lock_data = json.loads(self.path.read_text(encoding="utf-8"))
        except Exception:
            return (
                f"Another cbt process appears to be running. Existing lock file: {self.path}. "
                "The lock file could not be parsed, so it may be stale."
            )

        created = str(lock_data.get("created_utc", "unknown"))
        age_text = "unknown"
        try:
            age = (_dt.datetime.now(_dt.UTC) - parse_utc_iso(created)).total_seconds()
            age_text = format_age(age)
        except Exception:
            pass

        pid = lock_data.get("pid", "unknown")
        pid_state = "unknown"
        if isinstance(pid, int):
            alive = is_pid_alive(pid)
            if alive is True:
                pid_state = "alive"
            elif alive is False:
                pid_state = "not running"
        host = lock_data.get("host", "unknown")
        command = lock_data.get("command", [])
        if isinstance(command, list):
            command_text = shlex.join([str(entry) for entry in command]) if command else "unknown"
        else:
            command_text = str(command)

        if pid_state == "not running":
            intro = "A stale cbt workspace lock was found."
            outro = "The recorded PID is no longer running, so the lock can be removed manually."
        else:
            intro = "Another cbt process is already holding the workspace lock."
            outro = "If that process is gone, the lock is likely stale and can be removed manually."

        return (
            f"{intro}\n"
            f"Lock file: {self.path}\n"
            f"Created (UTC): {created}\n"
            f"Age: {age_text}\n"
            f"PID: {pid}\n"
            f"PID status: {pid_state}\n"
            f"Host: {host}\n"
            f"Command: {command_text}\n"
            f"{outro}"
        )


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def write_text(path: Path, content: str) -> None:
    ensure_dir(path.parent)
    path.write_text(content, encoding="utf-8")


def which(name: str) -> str | None:
    from shutil import which as _which

    return _which(name)
