from __future__ import annotations

import json
import shlex
from pathlib import Path

from .util import run


def _load_compile_db(build_dir: Path) -> list[dict]:
    path = build_dir / "compile_commands.json"
    if not path.exists():
        return []
    return json.loads(path.read_text(encoding="utf-8"))


def compile_sources(build_dir: Path, sources: list[Path], quiet: bool) -> None:
    db = _load_compile_db(build_dir)
    if not db:
        raise FileNotFoundError(str(build_dir / "compile_commands.json"))
    for source in sources:
        entry = _find_entry(db, source)
        if not entry:
            raise FileNotFoundError(f"No compile command for {source}")
        cmd = _command_from_entry(entry)
        if "-c" not in cmd:
            cmd.append("-c")
        run(cmd, cwd=Path(entry["directory"]), quiet=quiet)


def _find_entry(db: list[dict], source: Path) -> dict | None:
    source_str = str(source)
    for entry in db:
        if entry.get("file") == source_str:
            return entry
    source_abs = source.resolve()
    for entry in db:
        entry_file = Path(entry.get("file", ""))
        if not entry_file.is_absolute():
            entry_file = Path(entry["directory"]) / entry_file
        if entry_file.resolve() == source_abs:
            return entry
    return None


def _command_from_entry(entry: dict) -> list[str]:
    if "arguments" in entry:
        return list(entry["arguments"])
    return shlex.split(entry["command"])
