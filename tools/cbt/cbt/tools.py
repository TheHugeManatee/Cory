from __future__ import annotations

from .util import which


def resolve_tool(name: str, config: dict | None, fallback_names: list[str]) -> str | None:
    if config:
        tools = config.get("tools", {})
        if name in tools:
            return tools[name]
    for candidate in fallback_names:
        found = which(candidate)
        if found:
            return found
    return None
