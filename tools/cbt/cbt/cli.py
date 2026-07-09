from __future__ import annotations

import os
import sys
import shutil
from dataclasses import dataclass
from pathlib import Path
from typing import Any
import tempfile
import subprocess

import click

from . import analyze as analyze_mod
from . import cmake as cmake_mod
from . import compile as compile_mod
from . import conan as conan_mod
from . import ctest as ctest_mod
from . import format as format_mod
from . import git as git_mod
from . import run as run_mod
from . import sanitizers as sanitizers_mod
from . import tidy as tidy_mod
from . import visual_review as visual_review_mod
from .config import new_config, require_config, write_config
from .paths import (
    build_dir_for_profile,
    conan_home_for_profile,
    config_path,
    default_build_root,
    last_build_dir_path,
    last_profile_path,
    profile_lock_path,
    repo_root,
    venv_dir_for_build_root,
)
from .tools import resolve_tool
from .util import (
    CbtError,
    CbtLock,
    ConfigError,
    MissingPrereq,
    ToolError,
    ensure_dir,
    is_windows,
    print_json,
    run,
    which,
    write_text,
)
from shutil import which as _shutil_which


@dataclass
class CliContext:
    quiet: bool


def _python_executable() -> str:
    return sys.executable


def _venv_bin(venv_dir: Path) -> Path:
    return venv_dir / ("Scripts" if is_windows() else "bin")


def _venv_python(venv_dir: Path) -> Path:
    return _venv_bin(venv_dir) / ("python.exe" if is_windows() else "python")


def _tool_from_venv(venv_dir: Path, name: str) -> Path:
    suffix = ".exe" if is_windows() else ""
    return _venv_bin(venv_dir) / f"{name}{suffix}"


def _ensure_venv(venv_dir: Path, quiet: bool) -> None:
    if _venv_python(venv_dir).exists():
        return
    ensure_dir(venv_dir.parent)
    run([_python_executable(), "-m", "venv", str(venv_dir)], quiet=quiet)


def _venv_install(venv_dir: Path, packages: list[str], quiet: bool) -> None:
    python = _venv_python(venv_dir)
    run([str(python), "-m", "pip", "install", "--upgrade", "pip"], quiet=quiet)
    run([str(python), "-m", "pip", "install", *packages], quiet=quiet)


def _tool_paths(venv_dir: Path) -> dict[str, str]:
    tools: dict[str, str] = {
        "cmake": str(_tool_from_venv(venv_dir, "cmake")),
        "ctest": str(_tool_from_venv(venv_dir, "ctest")),
        "conan": str(_tool_from_venv(venv_dir, "conan")),
        "ninja": which("ninja") or "ninja",
        "cc": which("clang") or "clang",
        "cxx": which("clang++") or "clang++",
        "clang_tidy": which("clang-tidy-22") or which("clang-tidy") or "clang-tidy",
        "clang_format": which("clang-format") or "clang-format",
    }

    # On Windows, prefer clang-format/clang-tidy bundled with Visual Studio's LLVM tools
    if is_windows():
        try:
            inst = _find_vs_installation()
        except Exception:
            inst = None
        if inst:
            llvm_bin = Path(inst) / "VC" / "Tools" / "Llvm" / "bin"
            try:
                if llvm_bin.exists():
                    cf = llvm_bin / "clang-format.exe"
                    ct = llvm_bin / "clang-tidy.exe"
                    if cf.exists():
                        tools["clang_format"] = str(cf)
                    if ct.exists():
                        tools["clang_tidy"] = str(ct)
            except Exception:
                # Non-fatal: fall back to PATH-based discovery
                pass

    return tools


def _prepend_env_path(env: dict[str, str], key: str, value: str) -> None:
    if not value:
        return
    existing = env.get(key)
    if existing:
        env[key] = f"{value}{os.pathsep}{existing}"
    else:
        env[key] = value


def _append_colon_option(existing: str | None, option: str) -> str:
    current = existing or ""
    if not current:
        return option
    if option in current.split(":"):
        return current
    return f"{current}:{option}"


def _is_tsan_profile(config: dict) -> bool:
    profile = str(config.get("cbt", {}).get("profile", "")).lower()
    return "tsan" in profile


def _env_profile() -> str | None:
    return os.environ.get("CORY_BUILD_PROFILE")


def _resolve_profile(profile: str | None) -> str:
    return profile or _env_profile() or "codex"


def _argv_profile(argv: list[str]) -> str | None:
    for index, arg in enumerate(argv):
        if arg == "--profile" and index + 1 < len(argv):
            return argv[index + 1]
        prefix = "--profile="
        if arg.startswith(prefix):
            return arg[len(prefix):]
    return None


def _config_env(config: dict) -> dict[str, str]:
    env = dict(os.environ)
    conan_home = config.get("conan", {}).get("home")
    if conan_home:
        env["CONAN_HOME"] = str(conan_home)
    if _is_tsan_profile(config):
        suppressions = repo_root() / "tools" / "tsan" / "tsan.supp"
        if suppressions.exists():
            option = f"suppressions={suppressions}"
            env["TSAN_OPTIONS"] = _append_colon_option(env.get("TSAN_OPTIONS"), option)

    vulkan = config.get("vulkan")
    if vulkan and vulkan.get("sdk"):
        sdk = Path(vulkan["sdk"])
        env["VULKAN_SDK"] = str(sdk)
        bin_path = sdk / "bin"
        lib_path = sdk / "lib"
        _prepend_env_path(env, "PATH", str(bin_path))
        if not is_windows():
            _prepend_env_path(env, "LD_LIBRARY_PATH", str(lib_path))
            layer_dirs = [
                sdk / "share" / "vulkan" / "explicit_layer.d",
                sdk / "share" / "vulkan" / "implicit_layer.d",
                sdk / "etc" / "vulkan" / "explicit_layer.d",
                sdk / "etc" / "vulkan" / "implicit_layer.d",
            ]
            for layer_dir in layer_dirs:
                if layer_dir.exists():
                    _prepend_env_path(env, "VK_LAYER_PATH", str(layer_dir))
    return env


def _test_env(config: dict) -> dict[str, str]:
    env = _config_env(config)
    temp_candidates = [env.get("TMPDIR", ""), env.get("TEMP", ""), env.get("TMP", "")]
    if any(p.startswith("/mnt/") for p in temp_candidates if p):
        temp_root = Path("/tmp/cory-tests")
        ensure_dir(temp_root)
        env["TMPDIR"] = str(temp_root)
        env["TEMP"] = str(temp_root)
        env["TMP"] = str(temp_root)
    return env


def _parse_defines(values: tuple[str, ...]) -> dict[str, str]:
    defines: dict[str, str] = {}
    for item in values:
        if "=" not in item:
            raise ConfigError(f"Invalid define: {item}")
        key, value = item.split("=", 1)
        defines[key] = value
    return defines


def _load_or_fail(profile: str | None, build_root: Path | None) -> tuple[dict, Path]:
    root = build_root or default_build_root()
    env_profile = _env_profile()
    if profile:
        build_dir = build_dir_for_profile(profile, root)
    elif env_profile:
        build_dir = build_dir_for_profile(env_profile, root)
    else:
        last = last_build_dir_path(root)
        if last.exists():
            build_dir = Path(last.read_text(encoding="utf-8").strip())
        else:
            build_dir = build_dir_for_profile("codex", root)
    config = require_config(build_dir)
    return config, build_dir


def _record_last(build_root: Path, profile: str, build_dir: Path) -> None:
    write_text(last_profile_path(build_root), profile)
    write_text(last_build_dir_path(build_root), str(build_dir))


def _status_text_lines(data: dict[str, Any], prefix: str = "") -> list[str]:
    lines: list[str] = []
    for key, value in data.items():
        label = f"{prefix}{key}"
        if isinstance(value, dict):
            lines.append(f"[{label.strip(' ')}]")
            lines.extend(_status_text_lines(value, prefix=f"  {label}."))
        else:
            lines.append(f"{label}: {value}")
    return lines


def _sanitizer_config_dict(state: sanitizers_mod.SanitizerState) -> dict[str, str | bool]:
    return {
        "active": state.active_value,
        "signature": state.signature,
        "from_define": state.from_define,
    }


def _sanitizer_define_key(build_type: str) -> str:
    return f"CORY_SANITIZERS_{build_type}"


def _apply_sanitizer_cli_option(
    defines: dict[str, str],
    build_type: str,
    sanitizers: str | None,
    no_sanitizers: bool,
) -> dict[str, str]:
    updated = dict(defines)
    key = _sanitizer_define_key(build_type)
    if sanitizers is not None and no_sanitizers:
        raise ConfigError("Use either --sanitizers or --no-sanitizers, not both.")
    if sanitizers is not None:
        updated[key] = sanitizers
    elif no_sanitizers:
        updated[key] = ""
    return updated


def _resolve_sanitizer_state(
    *,
    build_type: str,
    defines: dict[str, str],
    stored_active: str | None,
) -> tuple[sanitizers_mod.SanitizerState, dict[str, str], dict[str, str]]:
    sanitizer_state = sanitizers_mod.resolve_sanitizers(
        build_type=build_type,
        defines=defines,
        stored_active=stored_active,
        windows=is_windows(),
    )
    synced_defines = dict(defines)
    synced_defines[_sanitizer_define_key(build_type)] = sanitizer_state.active_value
    return sanitizer_state, synced_defines, _effective_cmake_defines(
        synced_defines, sanitizer_state, build_type
    )


def _effective_cmake_defines(
    defines: dict[str, str],
    sanitizer_state: sanitizers_mod.SanitizerState,
    build_type: str,
) -> dict[str, str]:
    effective = dict(defines)
    effective.pop(_sanitizer_define_key(build_type), None)
    effective["CORY_ACTIVE_SANITIZERS"] = sanitizer_state.active_value
    effective["CORY_SANITIZERS_VIA_TOOLCHAIN"] = "ON"
    return effective


_KNOWN_SANITIZER_FLAGS = (
    "/fsanitize=address",
    "-fsanitize=address",
    "-fsanitize=thread",
    "-fsanitize=undefined",
)


def _read_cmake_cache_entries(build_dir: Path) -> dict[str, str]:
    cache_path = build_dir / "CMakeCache.txt"
    if not cache_path.exists():
        return {}

    entries: dict[str, str] = {}
    for line in cache_path.read_text(encoding="utf-8").splitlines():
        if not line or line.startswith(("//", "#")) or "=" not in line:
            continue
        key_type, value = line.split("=", 1)
        if ":" not in key_type:
            continue
        key, _ = key_type.split(":", 1)
        entries[key] = value
    return entries


def _cmake_cache_has_expected_sanitizer_flags(
    build_dir: Path, sanitizer_state: sanitizers_mod.SanitizerState
) -> bool:
    entries = _read_cmake_cache_entries(build_dir)
    if not entries:
        return True

    compile_values = " ".join(
        value
        for key, value in entries.items()
        if key.startswith("CMAKE_CXX_FLAGS") or key.startswith("CMAKE_C_FLAGS")
    )
    link_values = " ".join(
        value
        for key, value in entries.items()
        if key.startswith("CMAKE_EXE_LINKER_FLAGS")
        or key.startswith("CMAKE_SHARED_LINKER_FLAGS")
    )

    if sanitizer_state.enabled:
        has_compile_flags = all(flag in compile_values for flag in sanitizer_state.cxxflags)
        required_link_flags = tuple(
            dict.fromkeys(
                (*sanitizer_state.shared_link_flags, *sanitizer_state.exe_link_flags)
            )
        )
        has_link_flags = all(flag in link_values for flag in required_link_flags)
        return has_compile_flags and has_link_flags

    return not any(
        flag in compile_values or flag in link_values for flag in _KNOWN_SANITIZER_FLAGS
    )


def _reset_cmake_config_state(build_dir: Path, quiet: bool, reason: str) -> None:
    if not quiet:
        sys.stdout.write(f"Resetting CMake configure state: {reason}\n")

    for relative in (
        "CMakeCache.txt",
        "build.ninja",
        "cmake_install.cmake",
        "compile_commands.json",
        "CTestTestfile.cmake",
        "DartConfiguration.tcl",
        ".ninja_deps",
        ".ninja_log",
    ):
        path = build_dir / relative
        if path.exists():
            path.unlink()

    for relative in ("CMakeFiles", ".cmake", "Testing"):
        path = build_dir / relative
        if path.exists():
            shutil.rmtree(path)


def _ensure_cmake_sanitizer_state(
    *,
    tools: dict[str, str],
    config: dict,
    build_dir: Path,
    env: dict[str, str],
    quiet: bool,
    sanitizer_state: sanitizers_mod.SanitizerState,
) -> None:
    if _cmake_cache_has_expected_sanitizer_flags(build_dir, sanitizer_state):
        return

    reason = (
        f"stored sanitizer state '{sanitizer_state.active_value or 'none'}' "
        "does not match cached CMake flags"
    )
    _reset_cmake_config_state(build_dir, quiet, reason)
    cmake_mod.configure(
        tools["cmake"],
        build_dir,
        Path(config["paths"]["source_dir"]),
        config["cbt"]["build_type"],
        Path(config["cmake"]["toolchain_file"]),
        _effective_cmake_defines(
            dict(config["cmake"]["defines"]),
            sanitizer_state,
            config["cbt"]["build_type"],
        ),
        env,
        quiet,
    )


def _conan_conf_for_sanitizers(
    sanitizer_state: sanitizers_mod.SanitizerState,
) -> dict[str, list[str]]:
    if not sanitizer_state.enabled:
        return {}

    return {
        "tools.build:cxxflags": list(sanitizer_state.cxxflags),
        "tools.build:sharedlinkflags": list(sanitizer_state.shared_link_flags),
        "tools.build:exelinkflags": list(sanitizer_state.exe_link_flags),
    }


def _activate_build_env(
    config: dict,
    quiet: bool,
    *,
    base_env: dict[str, str] | None = None,
    allow_discovery: bool = True,
) -> dict[str, str]:
    env = dict(base_env) if base_env is not None else _config_env(config)
    if not is_windows():
        return env

    installation = config.get("msvc", {}).get("installation")
    cached_env = config.get("msvc", {}).get("env")
    if allow_discovery and not installation:
        try:
            installation = _find_vs_installation()
        except Exception:
            installation = None
    if installation:
        return _activate_vs_env_into(env, installation, "x64", quiet)
    return _ensure_msvc_dev_env(
        env,
        quiet,
        cached_installation=installation,
        cached_env=cached_env,
    )


def _capture_msvc_config_env(config: dict, env: dict[str, str]) -> None:
    if not is_windows():
        return

    installation = config.get("msvc", {}).get("installation")
    if not installation:
        return

    env_before_activation = _config_env(config)
    delta: dict[str, str] = {}
    for key, value in env.items():
        original = env_before_activation.get(key)
        if original != value:
            delta[key] = value
    config.setdefault("msvc", {})["env"] = delta


def _log_detected_compiler(env: dict[str, str], quiet: bool, prefix: str = "") -> None:
    if quiet or not is_windows():
        return
    try:
        cl_path = _shutil_which("cl", path=env.get("PATH", ""))
    except Exception:
        cl_path = None
    if not cl_path:
        cl_path = _find_executable_in_env_path(env.get("PATH", ""), "cl")
    if cl_path:
        message = f"{prefix}: cl detected in captured PATH: {cl_path}" if prefix else f"Using detected cl: {cl_path}"
        sys.stdout.write(f"{message}\n")


def _run_conan_install(
    *,
    tools: dict[str, str],
    build_dir: Path,
    source_dir: Path,
    profile: str,
    profile_host: str,
    profile_build: str,
    build_type: str,
    env: dict[str, str],
    quiet: bool,
    sanitizer_state: sanitizers_mod.SanitizerState,
) -> None:
    conan_mod.ensure_profile_detected(
        tools["conan"],
        quiet,
        env=env,
        requested_profiles=[profile_host, profile_build],
    )
    conan_mod.install(
        tools["conan"],
        build_dir,
        source_dir,
        profile_host,
        profile_build,
        build_type,
        quiet,
        preset_name=profile,
        env=env,
        conf=_conan_conf_for_sanitizers(sanitizer_state),
    )


def _targets_from_help(output: str) -> list[str]:
    targets: list[str] = []
    for line in output.splitlines():
        line = line.strip()
        if not line or line.startswith("The following are"):
            continue
        if line.startswith("..."):
            continue
        targets.append(line.split()[0])
    return sorted(set(targets))


def _build_test_targets(config: dict, build_dir: Path, env: dict[str, str], quiet: bool) -> None:
    target_help = run(
        [config["tools"]["cmake"], "--build", str(build_dir), "--target", "help"],
        env=env,
        quiet=True,
    )
    available = set(_targets_from_help(target_help.stdout))
    for target in ["tests", "Cory_Tests"]:
        if target in available:
            cmake_mod.build(
                config["tools"]["cmake"],
                build_dir,
                config["cbt"]["build_type"],
                target,
                None,
                False,
                env,
                quiet,
                native_tool=config.get("tools", {}).get("ninja"),
            )
            return
    cmake_mod.build(
        config["tools"]["cmake"],
        build_dir,
        config["cbt"]["build_type"],
        None,
        None,
        False,
        env,
        quiet,
        native_tool=config.get("tools", {}).get("ninja"),
    )


def _filter_source_files(files: list[Path]) -> list[Path]:
    exts = {".c", ".cc", ".cpp", ".cxx", ".h", ".hpp", ".hh", ".hxx", ".inl"}
    return [f for f in files if f.suffix.lower() in exts]


def _collect_files(
    paths: tuple[str, ...], repo: Path, allow_changed: bool, quiet: bool
) -> list[Path]:
    if paths:
        return [Path(p) for p in paths]
    if allow_changed:
        changed = [repo / p for p in git_mod.changed_files(repo, quiet)]
        return [p for p in changed if p.exists()]
    return []


def _format_size(num_bytes: int) -> str:
    units = ["B", "KB", "MB", "GB", "TB"]
    size = float(num_bytes)
    for unit in units:
        if size < 1024.0:
            return f"{size:.1f}{unit}"
        size /= 1024.0
    return f"{size:.1f}PB"


def _dir_size(path: Path) -> int:
    total = 0
    for root, _, files in os.walk(path):
        for name in files:
            try:
                total += (Path(root) / name).stat().st_size
            except FileNotFoundError:
                continue
    return total


def _msvc_env_active(env: dict[str, str]) -> bool:
    # Check whether 'cl' is available using the provided env's PATH
    path = env.get("PATH")
    return _shutil_which("cl", path=path) is not None


def _find_vs_installation() -> str | None:
    # Try vswhere in its typical location
    program_files_x86 = os.environ.get("ProgramFiles(x86)") or os.environ.get(
        "ProgramFiles"
    )
    if program_files_x86:
        vswhere = (
            Path(program_files_x86)
            / "Microsoft Visual Studio"
            / "Installer"
            / "vswhere.exe"
        )
        if vswhere.exists():
            try:
                res = run(
                    [
                        str(vswhere),
                        "-latest",
                        "-products",
                        "*",
                        "-requires",
                        "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                        "-property",
                        "installationPath",
                    ],
                    quiet=True,
                )
                inst = res.stdout.strip()
                if inst:
                    return inst
            except Exception:
                pass

    # Fallback: look for common installation folders and pick the newest
    if program_files_x86:
        base = Path(program_files_x86) / "Microsoft Visual Studio"
        if base.exists():
            # versions like 2017, 2019, 2022
            candidates = list(base.glob("*"))
            candidates = [p for p in candidates if (p / "Common7").exists()]
            if candidates:
                # return the one with highest name (best-effort)
                candidates.sort()
                return str(candidates[-1])
    return None


def _activate_vs_env_into(
    env: dict[str, str], inst_path: str, arch: str = "x64", quiet: bool = True
) -> dict[str, str]:
    # Try VsDevCmd.bat (newer) then vcvarsall.bat
    # Honor override environment variable CORY_VSDEV_BAT to point to an exact batch file
    override = os.environ.get("CORY_VSDEV_BAT")
    inst = Path(inst_path)
    # Candidate locations; prefer vcvarsall.bat then VsDevCmd.bat; allow override
    candidates: list[Path] = []
    if override:
        cand = Path(override)
        if cand.exists():
            if not quiet:
                sys.stdout.write(f"Using CORY_VSDEV_BAT override: {cand}\n")
            candidates = [cand]
        else:
            if not quiet:
                sys.stdout.write(
                    f"CORY_VSDEV_BAT override set but path not found: {override}\n"
                )
            candidates = []
    else:
        # Standard vcvarsall path
        std_vcvars = inst / "VC" / "Auxiliary" / "Build" / "vcvarsall.bat"
        std_vsdev = inst / "Common7" / "Tools" / "VsDevCmd.bat"
        found_vcvars: list[Path] = []
        found_vsdev: list[Path] = []
        try:
            for p in inst.rglob("vcvarsall.bat"):
                found_vcvars.append(p)
            for p in inst.rglob("VsDevCmd.bat"):
                found_vsdev.append(p)
        except Exception:
            pass
        # Prefer standard path if present
        if std_vcvars.exists():
            candidates.append(std_vcvars)
        candidates.extend([p for p in found_vcvars if p != std_vcvars])
        if std_vsdev.exists():
            candidates.append(std_vsdev)
        candidates.extend([p for p in found_vsdev if p != std_vsdev])

    for bat in candidates:
        if bat.exists():
            if not quiet:
                sys.stdout.write(f"Trying VS dev batch: {bat}\n")
            # Create a temporary batch file that calls the dev batch and then emits the environment
            # This avoids complex quoting issues when invoking cmd /c with embedded quotes.
            tmp = None
            try:
                fd, tmp_path = tempfile.mkstemp(suffix=".bat", text=True)
                tmp = Path(tmp_path)
                with os.fdopen(fd, "w", encoding="utf-8") as f:
                    f.write(f"@echo off\n")
                    # Use CALL to run the target batch and then print environment
                    # vcvarsall.bat expects 'x64' etc.; VsDevCmd.bat prefers '-arch=x64'
                    bat_name = bat.name.lower()
                    if bat_name == "vcvarsall.bat":
                        f.write(f'call "{str(bat)}" {arch}\n')
                    else:
                        f.write(f'call "{str(bat)}" -arch={arch}\n')
                    f.write("set\n")
                # Run the temp batch and capture stdout
                try:
                    comspec = os.environ.get("COMSPEC", "cmd")
                    # Use subprocess.run to avoid the run() quoting/printing behavior and capture stdout
                    stdout = ""
                    proc = subprocess.run(
                        [comspec, "/c", str(tmp)],
                        capture_output=True,
                        text=True,
                        check=True,
                    )
                    stdout = proc.stdout
                    if not quiet:
                        sys.stdout.write(
                            f"VsDev batch ran successfully, captured {len(stdout.splitlines())} env lines\n"
                        )
                finally:
                    try:
                        tmp.unlink()
                    except Exception:
                        pass

                new_env = dict(env)
                captured: dict[str, str] = {}
                for line in stdout.splitlines():
                    if "=" in line:
                        k, v = line.split("=", 1)
                        captured[k] = v
                        try:
                            captured[k.upper()] = v
                        except Exception:
                            pass
                # Merge PATH: prefer captured PATH first, but keep any original PATH entries so tools
                # available in the invoking console (e.g. Git unix utils) remain accessible.
                captured_path = (
                    captured.get("PATH") or captured.get("Path") or captured.get("path")
                )
                original_path = (
                    env.get("PATH") or env.get("Path") or env.get("path") or ""
                )
                if captured_path:
                    if original_path:
                        merged_path = f"{captured_path}{os.pathsep}{original_path}"
                    else:
                        merged_path = captured_path
                    new_env["PATH"] = merged_path
                # Import other captured variables into new_env (preserve original when absent)
                for k, v in captured.items():
                    if k.upper() == "PATH":
                        continue
                    new_env[k] = v
                    try:
                        new_env[k.upper()] = v
                    except Exception:
                        pass
                return new_env
            except Exception:
                # try next candidate
                if tmp and tmp.exists():
                    try:
                        tmp.unlink()
                    except Exception:
                        pass
                continue
    raise MissingPrereq(
        "Could not locate Visual Studio developer batch files (VsDevCmd.bat or vcvarsall.bat)"
    )


def _ensure_msvc_dev_env(
    env: dict[str, str],
    quiet: bool,
    cached_installation: str | None = None,
    cached_env: dict[str, str] | None = None,
) -> dict[str, str]:
    if not is_windows():
        return env
    # If cl is already available in PATH, assume dev env active
    if _msvc_env_active(env):
        return env
    # If we have a cached environment dict from a prior configure, apply it instead
    if cached_env:
        new_env = dict(env)
        # Merge PATH specially: prefer cached PATH entries first, then preserve existing
        cached_path = (
            cached_env.get("PATH") or cached_env.get("Path") or cached_env.get("path")
        )
        original_path = env.get("PATH") or env.get("Path") or env.get("path") or ""
        if cached_path:
            if original_path:
                merged = f"{cached_path}{os.pathsep}{original_path}"
            else:
                merged = cached_path
            new_env["PATH"] = merged
        # Apply other cached variables (override)
        for k, v in cached_env.items():
            if k.upper() == "PATH":
                continue
            new_env[k] = v
            try:
                new_env[k.upper()] = v
            except Exception:
                pass
        return new_env

    # If a cached installation path was provided, use it; otherwise try to locate one.
    inst = cached_installation or _find_vs_installation()
    if not inst:
        raise MissingPrereq(
            "Visual Studio installation not found; please run 'x64 Native Tools Command Prompt' or install Visual Studio with C++ workload"
        )
    if not quiet:
        sys.stdout.write(f"Found Visual Studio installation: {inst}\n")
    return _activate_vs_env_into(env, inst, "x64", quiet)


@click.group()
@click.option("--quiet", is_flag=True, help="Suppress command echo.")
@click.pass_context
def cli(ctx: click.Context, quiet: bool) -> None:
    ctx.obj = CliContext(quiet=quiet)
    lock_profile = _resolve_profile(_argv_profile(sys.argv[1:]))
    lock = CbtLock(profile_lock_path(lock_profile))
    lock.acquire()
    ctx.call_on_close(lock.release)


@cli.command(
    short_help="Create configuration and run CMake",
    help=(
        "Create a new build configuration. This runs Conan install and CMake configure, "
        "persists the resulting settings in .cbt/config.toml, and resolves the active "
        "sanitizer set from CORY_SANITIZERS_<build-type> defines before invoking either tool."
    ),
)
@click.option("--profile", help="Logical cbt profile name. Determines the build directory name.")
@click.option("--build-type", default="Debug", help="CMake/Conan build type to configure.")
@click.option(
    "--build-root",
    type=click.Path(path_type=Path),
    help="Override the build root. On Linux/WSL prefer a native filesystem path.",
)
@click.option(
    "--profile-host",
    help="Conan host profile to use. Defaults to 'default' on Windows and 'codex-clang' on Linux.",
)
@click.option(
    "--profile-build",
    default="default",
    help="Conan build profile to use for build requirements.",
)
@click.option("--cc", help="C compiler path/name to persist into the generated cbt config.")
@click.option("--cxx", help="C++ compiler path/name to persist into the generated cbt config.")
@click.option(
    "--cmake-define",
    multiple=True,
    help=(
        "Additional -D style cache definitions in KEY=VALUE form. "
        "Use this to override sanitizer settings such as CORY_SANITIZERS_Debug=ASAN."
    ),
)
@click.option(
    "--sanitizers",
    help=(
        "Sanitizer set for this build type, for example ASAN or ASAN;UBSAN. "
        "This updates the stored sanitizer state and the matching CORY_SANITIZERS_<build-type> define."
    ),
)
@click.option(
    "--no-sanitizers",
    is_flag=True,
    help="Disable sanitizers for this build type and sync the stored state.",
)
@click.option(
    "--vulkan-sdk",
    help="Override the Vulkan SDK root and inject matching include/library hints.",
)
@click.option(
    "--export-compile-commands/--no-export-compile-commands",
    default=True,
    help="Enable or disable generation of compile_commands.json.",
)
@click.option("--force", is_flag=True, help="Replace an existing .cbt config for the selected profile.")
@click.pass_obj
def configure(
    ctx: CliContext,
    profile: str | None,
    build_type: str,
    build_root: Path | None,
    profile_host: str | None,
    profile_build: str,
    cc: str | None,
    cxx: str | None,
    cmake_define: tuple[str, ...],
    sanitizers: str | None,
    no_sanitizers: bool,
    vulkan_sdk: str | None,
    export_compile_commands: bool,
    force: bool,
) -> None:
    profile = _resolve_profile(profile)
    root = build_root or default_build_root()
    build_dir = build_dir_for_profile(profile, root)
    cfg_path = config_path(build_dir)
    if cfg_path.exists() and not force:
        raise ConfigError(f"Config already exists: {cfg_path}")
    venv_dir = venv_dir_for_build_root(root)
    _ensure_venv(venv_dir, ctx.quiet)
    _venv_install(venv_dir, ["conan>=2.0", "cmake", "ninja", "click"], ctx.quiet)
    tools = _tool_paths(venv_dir)
    if cc:
        tools["cc"] = cc
    if cxx:
        tools["cxx"] = cxx
    defines = _apply_sanitizer_cli_option(
        _parse_defines(cmake_define), build_type, sanitizers, no_sanitizers
    )
    if export_compile_commands:
        defines["CMAKE_EXPORT_COMPILE_COMMANDS"] = "ON"
    if cc:
        defines["CMAKE_C_COMPILER"] = cc
    if cxx:
        defines["CMAKE_CXX_COMPILER"] = cxx
    vulkan_hints: dict[str, str] = {}
    if vulkan_sdk:
        vulkan_hints = {
            "Vulkan_INCLUDE_DIR": str(Path(vulkan_sdk) / "include"),
            "Vulkan_LIBRARY": str(
                Path(vulkan_sdk)
                / "lib"
                / ("vulkan-1.lib" if is_windows() else "libvulkan.so")
            ),
        }
        defines.update(vulkan_hints)
    sanitizer_state, defines, effective_defines = _resolve_sanitizer_state(
        build_type=build_type,
        defines=defines,
        stored_active=None,
    )
    host_profile = profile_host or ("default" if is_windows() else "codex-clang")
    conan_home = conan_home_for_profile(profile, root, build_type, sanitizer_state.signature)
    base_config = {
        "cbt": {"profile": profile},
        "conan": {"home": str(conan_home)},
    }
    if vulkan_sdk or vulkan_hints:
        base_config["vulkan"] = {
            "sdk": vulkan_sdk,
            "cmake_hints": dict(vulkan_hints),
        }
    installation: str | None = None
    if is_windows():
        try:
            installation = _find_vs_installation()
        except Exception:
            installation = None
        if installation:
            base_config["msvc"] = {"installation": installation}
    env = _activate_build_env(base_config, ctx.quiet)
    _run_conan_install(
        tools=tools,
        build_dir=build_dir,
        source_dir=repo_root(),
        profile=profile,
        profile_host=host_profile,
        profile_build=profile_build,
        build_type=build_type,
        env=env,
        quiet=ctx.quiet,
        sanitizer_state=sanitizer_state,
    )
    toolchain_file = build_dir / "conan_toolchain.cmake"
    config = new_config(
        profile=profile,
        build_type=build_type,
        source_dir=repo_root(),
        build_dir=build_dir,
        build_root=root,
        venv_dir=venv_dir,
        profile_host=host_profile,
        profile_build=profile_build,
        toolchain_file=toolchain_file,
        defines=defines,
        tools=tools,
        conan_home=conan_home,
        vulkan_sdk=vulkan_sdk,
        vulkan_hints=vulkan_hints,
        sanitizer=_sanitizer_config_dict(sanitizer_state),
    )
    if installation:
        config.setdefault("msvc", {})["installation"] = installation
    _capture_msvc_config_env(config, env)
    _log_detected_compiler(env, ctx.quiet)

    cmake_mod.configure(
        tools["cmake"],
        build_dir,
        repo_root(),
        build_type,
        toolchain_file,
        effective_defines,
        env,
        ctx.quiet,
    )
    write_config(build_dir, config)
    _record_last(root, profile, build_dir)


@cli.command(
    short_help="Re-run configuration (CMake/Conan) for a profile",
    help=(
        "Re-run CMake configure using the stored .cbt config. Conan install is triggered "
        "automatically when sanitizer settings change, or manually with --conan."
    ),
)
@click.option("--profile", help="Logical cbt profile name to reconfigure.")
@click.option(
    "--conan",
    "run_conan",
    is_flag=True,
    help="Force rerunning Conan install even if the sanitizer state did not change.",
)
@click.option(
    "--cmake-define",
    multiple=True,
    help=(
        "Override stored cache definitions in KEY=VALUE form for this reconfigure run. "
        "Changing CORY_SANITIZERS_<build-type> automatically refreshes Conan dependencies."
    ),
)
@click.option(
    "--sanitizers",
    help=(
        "Sanitizer set for this build type, for example ASAN or ASAN;UBSAN. "
        "This updates the stored sanitizer state and the matching CORY_SANITIZERS_<build-type> define."
    ),
)
@click.option(
    "--no-sanitizers",
    is_flag=True,
    help="Disable sanitizers for this build type and sync the stored state.",
)
@click.pass_obj
def reconfigure(
    ctx: CliContext,
    profile: str | None,
    run_conan: bool,
    cmake_define: tuple[str, ...],
    sanitizers: str | None,
    no_sanitizers: bool,
) -> None:
    profile = _resolve_profile(profile)
    config, build_dir = _load_or_fail(profile, None)
    tools = config["tools"]
    defines = dict(config["cmake"]["defines"])
    defines.update(
        _apply_sanitizer_cli_option(
            _parse_defines(cmake_define),
            config["cbt"]["build_type"],
            sanitizers,
            no_sanitizers,
        )
    )
    previous_active = config.get("sanitizers", {}).get("active")
    previous_home = str(config.get("conan", {}).get("home", ""))
    sanitizer_state, defines, effective_defines = _resolve_sanitizer_state(
        build_type=config["cbt"]["build_type"],
        defines=defines,
        stored_active=previous_active,
    )
    conan_home = conan_home_for_profile(
        config["cbt"]["profile"],
        Path(config["paths"]["build_root"]),
        config["cbt"]["build_type"],
        sanitizer_state.signature,
    )
    config["conan"]["home"] = str(conan_home)
    config["sanitizers"] = _sanitizer_config_dict(sanitizer_state)
    config["cmake"]["defines"] = defines
    env = _activate_build_env(config, ctx.quiet, allow_discovery=False)
    sanitizer_changed = sanitizer_state.active_value != (previous_active or "") or str(conan_home) != previous_home
    cache_stale = not _cmake_cache_has_expected_sanitizer_flags(build_dir, sanitizer_state)
    if run_conan or sanitizer_changed:
        _run_conan_install(
            tools=tools,
            build_dir=Path(config["paths"]["build_dir"]),
            source_dir=Path(config["paths"]["source_dir"]),
            profile=profile,
            profile_host=config["conan"]["profile_host"],
            profile_build=config["conan"]["profile_build"],
            build_type=config["cbt"]["build_type"],
            env=env,
            quiet=ctx.quiet,
            sanitizer_state=sanitizer_state,
        )
    if sanitizer_changed:
        _reset_cmake_config_state(
            build_dir,
            ctx.quiet,
            "sanitizer selection changed and CMake cache must be reinitialized",
        )
    elif cache_stale:
        _reset_cmake_config_state(
            build_dir,
            ctx.quiet,
            "sanitizer-enabled profile has stale CMake cache entries",
        )
    _log_detected_compiler(env, ctx.quiet, prefix="reconfigure")

    cmake_mod.configure(
        tools["cmake"],
        build_dir,
        Path(config["paths"]["source_dir"]),
        config["cbt"]["build_type"],
        Path(config["cmake"]["toolchain_file"]),
        effective_defines,
        env,
        ctx.quiet,
    )
    write_config(build_dir, config)


@cli.command(short_help="Build the project or a specific target")
@click.option("--profile")
@click.option("--build-root", type=click.Path(path_type=Path))
@click.option("--target")
@click.option("--jobs", type=int)
@click.option("--verbose", is_flag=True)
@click.pass_obj
def build(
    ctx: CliContext,
    profile: str | None,
    build_root: Path | None,
    target: str | None,
    jobs: int | None,
    verbose: bool,
) -> None:
    profile = _resolve_profile(profile)
    config, build_dir = _load_or_fail(profile, build_root)
    env = _activate_build_env(config, ctx.quiet, allow_discovery=False)
    sanitizer_state, _, _ = _resolve_sanitizer_state(
        build_type=config["cbt"]["build_type"],
        defines=dict(config["cmake"]["defines"]),
        stored_active=config.get("sanitizers", {}).get("active"),
    )
    _ensure_cmake_sanitizer_state(
        tools=config["tools"],
        config=config,
        build_dir=build_dir,
        env=env,
        quiet=ctx.quiet,
        sanitizer_state=sanitizer_state,
    )
    cmake_mod.build(
        config["tools"]["cmake"],
        build_dir,
        config["cbt"]["build_type"],
        target,
        jobs,
        verbose,
        env,
        ctx.quiet,
        native_tool=config.get("tools", {}).get("ninja"),
    )


@cli.command(name="run", short_help="Build (optional) and run a built target", context_settings={"ignore_unknown_options": True})
@click.option("--profile")
@click.option("--build-root", type=click.Path(path_type=Path))
@click.option("--working-dir", type=click.Path(path_type=Path))
@click.option("--no-build", is_flag=True)
@click.argument("target")
@click.argument("args", nargs=-1)
@click.pass_obj
def run_target(
    ctx: CliContext,
    profile: str | None,
    build_root: Path | None,
    working_dir: Path | None,
    no_build: bool,
    target: str,
    args: tuple[str, ...],
) -> None:
    profile = _resolve_profile(profile)
    config, build_dir = _load_or_fail(profile, build_root)
    env = _activate_build_env(config, ctx.quiet, allow_discovery=False)
    if not no_build:
        # fail fast so we never run stale output if the build fails.
        cmake_mod.build(
            config["tools"]["cmake"],
            build_dir,
            config["cbt"]["build_type"],
            target,
            None,
            False,
            env,
            ctx.quiet,
            native_tool=config.get("tools", {}).get("ninja"),
        )
    exe = build_dir / "bin" / (target + (".exe" if is_windows() else ""))
    if not exe.exists():
        raise ConfigError(f"Executable not found: {exe}")
    if working_dir:
        os.chdir(working_dir)
    run_mod.run_target(exe, list(args), env, ctx.quiet)


@cli.command(short_help="Compile a shader using slangc")
@click.argument("shader", type=click.Path(path_type=Path))
@click.option(
    "--out",
    "out",
    "-o",
    type=click.Path(path_type=Path),
    default=None,
    help="Output .spv path; if omitted output is discarded",
)
@click.option("--entry", default="main", help="Entry point name")
@click.pass_obj
def slang(ctx: CliContext, shader: Path, out: Path | None, entry: str) -> None:
    """Compile a Slang shader file to SPIR-V using slangc.

    The stage is inferred from the filename suffix (e.g. .comp.slang -> compute).
    The include path <repo_root>/data/shaders is always added.
    """
    name = shader.name.lower()
    stage: str | None = None
    if name.endswith(".comp.slang"):
        stage = "compute"
    elif name.endswith(".vert.slang"):
        stage = "vertex"
    elif name.endswith(".frag.slang"):
        stage = "fragment"
    elif name.endswith(".geom.slang"):
        stage = "geometry"
    elif name.endswith(".tesc.slang"):
        stage = "tesscontrol"
    elif name.endswith(".tese.slang"):
        stage = "tesseval"

    include_dir = str(repo_root() / "data" / "shaders")

    # If we couldn't infer a stage, treat the file as a Slang module/library.
    module_mode = stage is None

    if module_mode:
        # Prepare output path: use provided --out or a temporary file which we'll delete.
        if out:
            module_out = Path(out)
            ensure_dir(module_out.parent)
        else:
            tf = tempfile.NamedTemporaryFile(
                prefix="slang-module-", suffix=".slang-module", delete=False
            )
            module_out = Path(tf.name)
            tf.close()

        cmd = [
            "slangc",
            str(shader),
            "-o",
            str(module_out),
            "-I",
            include_dir,
            "-warnings-as-errors",
            "all",
        ]
    else:
        cmd = [
            "slangc",
            str(shader),
            "-target",
            "spirv",
            "-entry",
            entry,
            "-stage",
            stage,
            "-I",
            include_dir,
            "-warnings-as-errors",
            "all",
        ]

    try:
        proc = subprocess.run(cmd, cwd=repo_root(), capture_output=True, check=True)
    except subprocess.CalledProcessError as e:
        stderr_text = e.stderr.decode(errors="replace") if e.stderr else ""
        sys.stderr.write(stderr_text)
        raise ToolError("slangc failed") from e

    if module_mode:
        if out:
            if not ctx.quiet:
                sys.stdout.write(f"Wrote Slang module: {module_out}\n")
        else:
            # No --out specified: remove the temp file and report success.
            try:
                module_out.unlink()
            except Exception:
                pass
            if not ctx.quiet:
                sys.stdout.write("slangc succeeded (module output discarded)\n")
    else:
        out_bytes = proc.stdout or b""
        if out:
            out_path = Path(out)
            ensure_dir(out_path.parent)
            with open(out_path, "wb") as f:
                f.write(out_bytes)
            if not ctx.quiet:
                sys.stdout.write(f"Wrote SPIR-V: {out_path}\n")
        else:
            if not ctx.quiet:
                sys.stdout.write("slangc succeeded (output discarded)\n")


@cli.command(name="targets", short_help="List available build targets")
@click.option("--profile")
@click.option("--build-root", type=click.Path(path_type=Path))
@click.option("--json", "as_json", is_flag=True)
@click.pass_obj
def list_targets(
    ctx: CliContext, profile: str | None, build_root: Path | None, as_json: bool
) -> None:
    profile = _resolve_profile(profile)
    config, build_dir = _load_or_fail(profile, build_root)
    env = _activate_build_env(config, ctx.quiet, allow_discovery=False)
    result = run(
        [config["tools"]["cmake"], "--build", str(build_dir), "--target", "help"],
        env=env,
        quiet=ctx.quiet,
    )
    targets = _targets_from_help(result.stdout)
    if as_json:
        print_json({"targets": targets})
    else:
        for t in targets:
            sys.stdout.write(f"{t}\n")


@cli.command(name="tests", short_help="List available tests")
@click.option("--profile")
@click.option("--build-root", type=click.Path(path_type=Path))
@click.option("--json", "as_json", is_flag=True)
@click.pass_obj
def list_tests(
    ctx: CliContext, profile: str | None, build_root: Path | None, as_json: bool
) -> None:
    profile = _resolve_profile(profile)
    config, build_dir = _load_or_fail(profile, build_root)
    env = _activate_build_env(
        config, ctx.quiet, base_env=_test_env(config), allow_discovery=False
    )
    tests = ctest_mod.list_tests(config["tools"]["ctest"], build_dir, env, ctx.quiet)
    if as_json:
        print_json({"tests": tests})
    else:
        for t in tests:
            sys.stdout.write(f"{t}\n")


@cli.command(name="test", short_help="Run tests (ctest) with filters/options")
@click.option("--profile")
@click.option("--build-root", type=click.Path(path_type=Path))
@click.option("--label")
@click.option("--repeat", type=int)
@click.option("--until-fail", type=int)
@click.option("--verbose", is_flag=True)
@click.option("--gdb", is_flag=True)
@click.option("--lldb", is_flag=True)
@click.option("--timeout", type=int)
@click.argument("regex", required=False, default=".*")
@click.pass_obj
def run_test(
    ctx: CliContext,
    profile: str | None,
    build_root: Path | None,
    label: str | None,
    repeat: int | None,
    until_fail: int | None,
    verbose: bool,
    gdb: bool,
    lldb: bool,
    timeout: int | None,
    regex: str,
) -> None:
    profile = _resolve_profile(profile)
    config, build_dir = _load_or_fail(profile, build_root)
    env = _activate_build_env(
        config, ctx.quiet, base_env=_test_env(config), allow_discovery=False
    )
    _build_test_targets(config, build_dir, env, ctx.quiet)
    ctest_mod.run_tests(
        config["tools"]["ctest"],
        build_dir,
        regex,
        label,
        repeat,
        until_fail,
        verbose,
        gdb,
        lldb,
        timeout,
        env,
        ctx.quiet,
    )


@cli.command(short_help="Compile given source files quickly")
@click.option("--profile")
@click.option("--build-root", type=click.Path(path_type=Path))
@click.argument("sources", nargs=-1, required=True)
@click.pass_obj
def compile(
    ctx: CliContext,
    profile: str | None,
    build_root: Path | None,
    sources: tuple[str, ...],
) -> None:
    profile = _resolve_profile(profile)
    config, build_dir = _load_or_fail(profile, build_root)
    compile_mod.compile_sources(build_dir, [Path(s) for s in sources], ctx.quiet)


@cli.command(short_help="Run static analysis (clang-tidy) on sources")
@click.option("--profile")
@click.option("--build-root", type=click.Path(path_type=Path))
@click.option("--checks")
@click.option("--fix", is_flag=True)
@click.argument("sources", nargs=-1, required=True)
@click.pass_obj
def analyze(
    ctx: CliContext,
    profile: str | None,
    build_root: Path | None,
    checks: str | None,
    fix: bool,
    sources: tuple[str, ...],
) -> None:
    profile = _resolve_profile(profile)
    config, build_dir = _load_or_fail(profile, build_root)
    analyze_mod.analyze_files(
        config["tools"]["clang_tidy"],
        build_dir,
        [Path(s) for s in sources],
        checks,
        fix,
        ctx.quiet,
    )


@cli.command(short_help="Format source files using clang-format")
@click.option("--profile")
@click.option("--build-root", type=click.Path(path_type=Path))
@click.option("--check", is_flag=True)
@click.option(
    "--branch",
    type=str,
    default=None,
    help="Diff files against this branch (e.g. develop)",
)
@click.argument("paths", nargs=-1)
@click.pass_obj
def fmt(
    ctx: CliContext,
    profile: str | None,
    build_root: Path | None,
    check: bool,
    branch: str | None,
    paths: tuple[str, ...],
) -> None:
    profile = _resolve_profile(profile)
    config, build_dir = _load_or_fail(profile, build_root)
    repo = repo_root()
    if paths:
        files = [Path(p) for p in paths]
    elif branch:
        changed = [
            repo / p for p in git_mod.changed_files_against(repo, branch, ctx.quiet)
        ]
        files = [p for p in changed if p.exists()]
    else:
        files = _collect_files(paths, repo, True, ctx.quiet)

    files = _filter_source_files(files)
    if not files:
        raise ConfigError("No files to format")
    format_mod.format_files(config["tools"]["clang_format"], files, check, ctx.quiet)


@cli.command(short_help="Review golden image changes or open visual review requests")
@click.option("--profile")
@click.option("--build-root", type=click.Path(path_type=Path))
@click.option("--scan", is_flag=True, help="Open review requests found under the build root")
@click.option("--upstream", is_flag=True, help="Compare against the develop branch instead of HEAD")
@click.option("--all", "run_all", is_flag=True, help="Build and run all Catch2 tests tagged [visual] with interactive review enabled")
@click.pass_obj
def visual_review(
    ctx: CliContext,
    profile: str | None,
    build_root: Path | None,
    scan: bool,
    upstream: bool,
    run_all: bool,
) -> None:
    if sum(1 for flag in (scan, upstream, run_all) if flag) > 1:
        raise ConfigError("Use at most one of --scan, --upstream, or --all.")

    profile = _resolve_profile(profile)
    config, build_dir = _load_or_fail(profile, build_root)
    review_root = build_root or default_build_root()
    repo = repo_root()
    reference_ref = "develop" if upstream else "HEAD"

    env = _activate_build_env(config, ctx.quiet, base_env=_test_env(config), allow_discovery=False)
    sanitizer_state, _, _ = _resolve_sanitizer_state(
        build_type=config["cbt"]["build_type"],
        defines=dict(config["cmake"]["defines"]),
        stored_active=config.get("sanitizers", {}).get("active"),
    )
    _ensure_cmake_sanitizer_state(
        tools=config["tools"],
        config=config,
        build_dir=build_dir,
        env=env,
        quiet=ctx.quiet,
        sanitizer_state=sanitizer_state,
    )

    if run_all:
        visual_tests = visual_review_mod.discover_visual_tests(repo)
        if not visual_tests:
            sys.stdout.write("No [visual] tests were found\n")
            return
        env["CORY_VISUAL_INTERACTIVE"] = "1"
        _build_test_targets(config, build_dir, env, ctx.quiet)
        ctest_mod.run_tests(
            config["tools"]["ctest"],
            build_dir,
            visual_review_mod.visual_test_regex(visual_tests),
            None,
            None,
            None,
            False,
            False,
            False,
            None,
            env,
            ctx.quiet,
        )
        return

    if scan:
        requests = visual_review_mod.discover_open_requests(review_root)
        if not requests:
            sys.stdout.write(f"No open visual review requests found under {review_root}\n")
            return
    else:
        source_paths = visual_review_mod.discover_worktree_image_changes(repo, reference_ref, ctx.quiet)
        if not source_paths:
            sys.stdout.write(f"No changed golden images found against {reference_ref}\n")
            return

    cmake_mod.build(
        config["tools"]["cmake"],
        build_dir,
        config["cbt"]["build_type"],
        "VisualDiffReviewer",
        None,
        False,
        env,
        ctx.quiet,
        native_tool=config.get("tools", {}).get("ninja"),
    )

    reviewer_exe = build_dir / "bin" / ("VisualDiffReviewer" + (".exe" if is_windows() else ""))
    if not reviewer_exe.exists():
        raise ConfigError(f"VisualDiffReviewer executable not found: {reviewer_exe}")

    visual_review_mod.review_visual_changes(
        repo_root=repo,
        build_root=review_root,
        reviewer_executable=reviewer_exe,
        env=env,
        quiet=ctx.quiet,
        upstream=upstream,
        scan=scan,
    )


@cli.command(short_help="Run clang-tidy checks (lint) on files")
@click.option("--profile")
@click.option("--build-root", type=click.Path(path_type=Path))
@click.option("--checks")
@click.option("--warnings-as-errors")
@click.argument("paths", nargs=-1)
@click.pass_obj
def lint(
    ctx: CliContext,
    profile: str | None,
    build_root: Path | None,
    checks: str | None,
    warnings_as_errors: str | None,
    paths: tuple[str, ...],
) -> None:
    profile = _resolve_profile(profile)
    config, build_dir = _load_or_fail(profile, build_root)
    repo = repo_root()
    files = _filter_source_files(_collect_files(paths, repo, True, ctx.quiet))
    if not files:
        raise ConfigError("No files to lint")
    tidy_mod.tidy_files(
        config["tools"]["clang_tidy"],
        build_dir,
        files,
        checks,
        warnings_as_errors,
        False,
        ctx.quiet,
    )


@cli.command(short_help="Remove a profile's build directory")
@click.option("--profile")
@click.option("--build-root", type=click.Path(path_type=Path))
@click.option("--yes", is_flag=True)
@click.option("--confirm")
@click.pass_obj
def clean(
    ctx: CliContext,
    profile: str | None,
    build_root: Path | None,
    yes: bool,
    confirm: str | None,
) -> None:
    profile = _resolve_profile(profile)
    config, build_dir = _load_or_fail(profile, build_root)
    resolved_profile = config.get("cbt", {}).get("profile")
    if not yes and confirm != resolved_profile:
        raise ConfigError("Confirm with --yes or --confirm <profile>")
    if build_dir.exists():
        shutil.rmtree(build_dir)


@cli.group(short_help="Manage cached build roots and related actions")
def cache() -> None:
    pass


@cache.command("show", short_help="Show build cache contents")
@click.pass_obj
def cache_show(ctx: CliContext) -> None:
    root = default_build_root()
    if not root.exists():
        sys.stdout.write(f"{root} (missing)\n")
        return
    for item in root.iterdir():
        if item.is_dir():
            size = _format_size(_dir_size(item))
            sys.stdout.write(f"{item} ({size})\n")
        else:
            sys.stdout.write(f"{item}\n")


@cache.command("prune", short_help="Prune unused build cache entries")
@click.option("--yes", is_flag=True)
@click.pass_obj
def cache_prune(ctx: CliContext, yes: bool) -> None:
    if not yes:
        raise ConfigError("Confirm with --yes")
    root = default_build_root()
    if not root.exists():
        return
    for item in root.iterdir():
        if item.name.startswith("."):
            continue
        if not (item / ".cbt" / "config.toml").exists() and item.is_dir():
            shutil.rmtree(item)


@cache.command("conan", short_help="Clean Conan package cache")
@click.option("--yes", is_flag=True)
@click.pass_obj
def cache_conan(ctx: CliContext, yes: bool) -> None:
    if not yes:
        raise ConfigError("Confirm with --yes")
    conan = which("conan")
    if not conan:
        raise MissingPrereq("conan not found")
    run([conan, "cache", "clean", "--all"], quiet=ctx.quiet)


@cli.command(short_help="Run environment and tool checks (doctor)")
@click.option("--json", "as_json", is_flag=True)
@click.pass_obj
def doctor(ctx: CliContext, as_json: bool) -> None:
    checks: list[dict[str, Any]] = []
    tools = {
        "python": _python_executable(),
        "cmake": which("cmake"),
        "ninja": which("ninja"),
        "conan": which("conan"),
        "clang++": which("clang++") or which("clang++-22"),
        "clang-tidy": which("clang-tidy") or which("clang-tidy-22"),
        "clang-format": which("clang-format"),
        "glslangValidator": which("glslangValidator"),
    }
    missing = False
    for name, path in tools.items():
        if not path:
            missing = True
        checks.append(
            {
                "name": name,
                "status": "OK" if path else "Missing",
                "path": path or "",
                "fix": "" if path else f"Install {name}",
            }
        )
    if as_json:
        print_json({"checks": checks})
    else:
        for c in checks:
            sys.stdout.write(f"{c['status']}: {c['name']} {c['path']}\n")
    if missing:
        raise MissingPrereq("Missing prerequisites detected")


@cli.command(
    short_help="Show status information for a build/profile",
    help="Show the stored cbt configuration, including tool paths, Conan settings, and active sanitizers.",
)
@click.option("--profile", help="Logical cbt profile name to inspect.")
@click.option(
    "--build-root",
    type=click.Path(path_type=Path),
    help="Override the build root used to resolve the profile directory.",
)
@click.option("--json", "as_json", is_flag=True, help="Emit machine-readable JSON instead of text output.")
@click.pass_obj
def status(
    ctx: CliContext, profile: str | None, build_root: Path | None, as_json: bool
) -> None:
    profile = _resolve_profile(profile)
    config, build_dir = _load_or_fail(profile, build_root)
    build_type = str(config.get("cbt", {}).get("build_type", "Debug"))
    stored_defines = dict(config.get("cmake", {}).get("defines", {}))
    stored_active = config.get("sanitizers", {}).get("active")
    sanitizer_state, synced_defines, effective_defines = _resolve_sanitizer_state(
        build_type=build_type,
        defines=stored_defines,
        stored_active=stored_active,
    )
    config_file = config_path(build_dir)
    conan_info = dict(config.get("conan", {}))
    conan_home = Path(
        conan_info.get("home")
        or conan_home_for_profile(
            profile,
            Path(config["paths"]["build_root"]),
            build_type,
            sanitizer_state.signature,
        )
    )
    conan_info["home"] = str(conan_home)
    if conan_home:
        conan_info["home_exists"] = conan_home.exists()
        conan_info["settings_file"] = str(conan_home / "settings.yml")
        conan_info["settings_exists"] = (conan_home / "settings.yml").exists()
        conan_info["profiles_dir"] = str(conan_home / "profiles")
        conan_info["profiles_dir_exists"] = (conan_home / "profiles").exists()

    compile_commands = Path(build_dir) / "compile_commands.json"
    info = {
        "profile": profile,
        "config_path": str(config_file),
        "config_exists": config_file.exists(),
        "cbt": config.get("cbt", {}),
        "paths": config.get("paths", {}),
        "cmake": {
            **config.get("cmake", {}),
            "stored_defines": synced_defines,
            "effective_defines": effective_defines,
        },
        "tools": config.get("tools", {}),
        "conan": conan_info,
        "sanitizers": {
            **config.get("sanitizers", {}),
            "active": sanitizer_state.active_value,
            "signature": sanitizer_state.signature,
            "from_define": sanitizer_state.from_define,
            "resolved": sanitizer_state.enabled,
            "cxxflags": list(sanitizer_state.cxxflags),
            "shared_link_flags": list(sanitizer_state.shared_link_flags),
            "exe_link_flags": list(sanitizer_state.exe_link_flags),
        },
        "compile_commands": {
            "path": str(compile_commands),
            "exists": compile_commands.exists(),
        },
    }
    if as_json:
        print_json(info)
    else:
        sys.stdout.write("\n".join(_status_text_lines(info)) + "\n")


@cli.command(name="which", short_help="Resolve the path to a tool")
@click.argument("tool")
@click.pass_obj
def which_cmd(ctx: CliContext, tool: str) -> None:
    config = None
    try:
        config, _ = _load_or_fail(None, None)
    except ConfigError:
        config = None
    resolved = resolve_tool(tool, config, [tool])
    if not resolved:
        raise MissingPrereq(f"{tool} not found")
    sys.stdout.write(f"{resolved}\n")


@cli.command(short_help="Print environment variables for a profile")
@click.option("--profile")
@click.option("--build-root", type=click.Path(path_type=Path))
@click.pass_obj
def env(ctx: CliContext, profile: str | None, build_root: Path | None) -> None:
    profile = _resolve_profile(profile)
    config, _ = _load_or_fail(profile, build_root)
    env = _activate_build_env(config, ctx.quiet, allow_discovery=False)
    for key in ("VULKAN_SDK",):
        if key in env:
            sys.stdout.write(f"{key}={env[key]}\n")
    sys.stdout.write(f"PATH={env.get('PATH','')}\n")


def main() -> None:
    try:
        cli(standalone_mode=False)
    except CbtError as exc:
        sys.stderr.write(f"{exc}\n")
        raise SystemExit(exc.exit_code)
    except click.UsageError as exc:
        sys.stderr.write(f"{exc}\n")
        raise SystemExit(1)


def _find_executable_in_env_path(env_path: str, name: str) -> str | None:
    """Search the PATH string from an env dict for an executable name. Returns full path or None."""
    if not env_path:
        return None
    parts = env_path.split(os.pathsep)
    # Try name and name.exe
    candidates = [name, name + ".exe"]
    for p in parts:
        if not p:
            continue
        for cand in candidates:
            try:
                path = Path(p) / cand
                if path.exists():
                    return str(path)
            except Exception:
                continue
    return None
