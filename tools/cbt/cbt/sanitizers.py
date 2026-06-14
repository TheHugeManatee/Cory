from __future__ import annotations

from dataclasses import dataclass
from typing import Mapping

from .util import ConfigError

_ORDER = ("ASAN", "TSAN", "UBSAN")
_ALIASES = {
    "ADDRESS": "ASAN",
    "ASAN": "ASAN",
    "THREAD": "TSAN",
    "TSAN": "TSAN",
    "UNDEFINED": "UBSAN",
    "UBSAN": "UBSAN",
}


@dataclass(frozen=True)
class SanitizerState:
    active: tuple[str, ...]
    active_value: str
    signature: str
    cxxflags: tuple[str, ...]
    shared_link_flags: tuple[str, ...]
    exe_link_flags: tuple[str, ...]
    from_define: bool
    source_key: str

    @property
    def enabled(self) -> bool:
        return bool(self.active)


def _normalize_tokens(entries: list[str]) -> tuple[str, ...]:
    normalized: list[str] = []
    for entry in entries:
        stripped = entry.strip()
        if not stripped:
            continue
        upper = stripped.upper()
        if upper not in _ALIASES:
            raise ConfigError(
                f"Unknown sanitizer '{entry}'. Supported values are ASAN, TSAN, and UBSAN."
            )
        normalized.append(_ALIASES[upper])

    deduped = sorted(set(normalized), key=_ORDER.index)
    return tuple(deduped)


def parse_sanitizers(value: str | None) -> tuple[str, ...]:
    if value is None:
        return ()
    return _normalize_tokens(value.split(";"))


def default_sanitizers(build_type: str, windows: bool) -> tuple[str, ...]:
    if build_type != "Debug":
        return ()
    if windows:
        return ("ASAN",)
    return ("ASAN", "UBSAN")


def _validate(active: tuple[str, ...], windows: bool, source_key: str) -> None:
    has_asan = "ASAN" in active
    has_tsan = "TSAN" in active

    if has_asan and has_tsan:
        raise ConfigError(f"{source_key} cannot enable ASAN and TSAN at the same time.")

    if windows:
        unsupported = [entry for entry in active if entry != "ASAN"]
        if unsupported:
            joined = ";".join(unsupported)
            raise ConfigError(
                f"{source_key} requests unsupported sanitizer(s) on Windows/MSVC: {joined}. "
                "Only ASAN is supported."
            )
        if len(active) > 1:
            raise ConfigError(
                f"{source_key} requests unsupported sanitizer combination on Windows/MSVC: "
                f"{';'.join(active)}. Only ASAN is supported."
            )


def _signature(active: tuple[str, ...]) -> str:
    if not active:
        return "none"
    return "-".join(entry.lower() for entry in active)


def _flags(
    active: tuple[str, ...], windows: bool
) -> tuple[tuple[str, ...], tuple[str, ...], tuple[str, ...]]:
    if not active:
        return (), (), ()

    if windows:
        flags = ("/fsanitize=address",)
        return flags, flags, flags

    mapped: list[str] = []
    for entry in active:
        if entry == "ASAN":
            mapped.append("-fsanitize=address")
        elif entry == "TSAN":
            mapped.append("-fsanitize=thread")
        elif entry == "UBSAN":
            mapped.append("-fsanitize=undefined")

    cxxflags = list(mapped)
    if "ASAN" in active or "TSAN" in active:
        cxxflags.append("-fno-omit-frame-pointer")

    return tuple(cxxflags), tuple(mapped), tuple(mapped)


def resolve_sanitizers(
    *,
    build_type: str,
    defines: Mapping[str, str],
    stored_active: str | None,
    windows: bool,
) -> SanitizerState:
    key = f"CORY_SANITIZERS_{build_type}"
    if key in defines:
        active = parse_sanitizers(defines[key])
        from_define = True
        source_key = key
    elif stored_active is not None:
        active = parse_sanitizers(stored_active)
        from_define = False
        source_key = "stored sanitizer config"
    else:
        active = default_sanitizers(build_type, windows)
        from_define = False
        source_key = f"default sanitizer policy for {build_type}"

    _validate(active, windows, source_key)
    active_value = ";".join(active)
    cxxflags, shared_link_flags, exe_link_flags = _flags(active, windows)

    return SanitizerState(
        active=active,
        active_value=active_value,
        signature=_signature(active),
        cxxflags=cxxflags,
        shared_link_flags=shared_link_flags,
        exe_link_flags=exe_link_flags,
        from_define=from_define,
        source_key=source_key,
    )
