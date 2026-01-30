from __future__ import annotations

import datetime as _dt
import json
from pathlib import Path

from .paths import config_path
from .util import ConfigError, read_text, write_text


def _loads_toml(text: str) -> dict:
    try:
        import tomllib
    except ModuleNotFoundError:  # pragma: no cover
        raise ConfigError("tomllib not available (requires Python 3.11+)")
    return tomllib.loads(text)


def load_config(build_dir: Path) -> dict:
    path = config_path(build_dir)
    if not path.exists():
        raise ConfigError(f"Missing config: {path}")
    return _loads_toml(read_text(path))


def write_config(build_dir: Path, config: dict) -> None:
    path = config_path(build_dir)
    write_text(path, _dumps_toml(config))


def require_config(build_dir: Path) -> dict:
    return load_config(build_dir)


def new_config(
    *,
    profile: str,
    build_type: str,
    source_dir: Path,
    build_dir: Path,
    build_root: Path,
    venv_dir: Path,
    profile_host: str,
    profile_build: str,
    toolchain_file: Path,
    defines: dict[str, str],
    tools: dict[str, str],
    vulkan_sdk: str | None,
    vulkan_hints: dict[str, str],
) -> dict:
    config: dict = {
        "cbt": {
            "profile": profile,
            "build_type": build_type,
            "last_used_utc": _dt.datetime.utcnow().replace(microsecond=0).isoformat() + "Z",
        },
        "paths": {
            "source_dir": str(source_dir),
            "build_dir": str(build_dir),
            "build_root": str(build_root),
            "venv_dir": str(venv_dir),
        },
        "conan": {
            "profile_host": profile_host,
            "profile_build": profile_build,
            "build_missing": True,
        },
        "cmake": {
            "generator": "Ninja",
            "toolchain_file": str(toolchain_file),
            "export_compile_commands": True,
            "defines": dict(defines),
        },
        "tools": dict(tools),
    }
    if vulkan_sdk or vulkan_hints:
        config["vulkan"] = {
            "sdk": vulkan_sdk,
            "cmake_hints": dict(vulkan_hints),
        }
    return config


def _dumps_toml(config: dict) -> str:
    lines: list[str] = []

    def write_table(key: str, table: dict) -> None:
        lines.append(f"[{key}]")
        for k, v in table.items():
            lines.append(f"{k} = {format_value(v)}")
        lines.append("")

    def format_value(v) -> str:
        if isinstance(v, bool):
            return "true" if v else "false"
        if isinstance(v, (int, float)):
            return str(v)
        if isinstance(v, dict):
            items = ", ".join(f"{json.dumps(str(k))} = {format_value(val)}" for k, val in v.items())
            return "{ " + items + " }"
        return json.dumps(str(v))

    for section in ("cbt", "paths", "conan", "cmake", "tools", "vulkan"):
        if section in config:
            write_table(section, config[section])
    return "\n".join(lines).rstrip() + "\n"
