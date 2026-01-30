from __future__ import annotations

import os
import sys
import shutil
from dataclasses import dataclass
from pathlib import Path
from typing import Any

import click

from . import analyze as analyze_mod
from . import cmake as cmake_mod
from . import compile as compile_mod
from . import conan as conan_mod
from . import ctest as ctest_mod
from . import format as format_mod
from . import git as git_mod
from . import run as run_mod
from . import tidy as tidy_mod
from .config import load_config, new_config, require_config, write_config
from .paths import (
    build_dir_for_profile,
    config_path,
    default_build_root,
    last_build_dir_path,
    last_profile_path,
    repo_root,
    venv_dir_for_build_root,
)
from .tools import resolve_tool
from .util import (
    CbtError,
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
    return {
        "cmake": str(_tool_from_venv(venv_dir, "cmake")),
        "ctest": str(_tool_from_venv(venv_dir, "ctest")),
        "conan": str(_tool_from_venv(venv_dir, "conan")),
        "ninja": which("ninja") or "ninja",
        "cc": which("clang") or "clang",
        "cxx": which("clang++") or "clang++",
        "clang_tidy": which("clang-tidy-22") or which("clang-tidy") or "clang-tidy",
        "clang_format": which("clang-format") or "clang-format",
    }


def _prepend_env_path(env: dict[str, str], key: str, value: str) -> None:
    if not value:
        return
    existing = env.get(key)
    if existing:
        env[key] = f"{value}{os.pathsep}{existing}"
    else:
        env[key] = value


def _config_env(config: dict) -> dict[str, str]:
    env = dict(os.environ)
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
    if profile:
        build_dir = build_dir_for_profile(profile, root)
    else:
        last = last_build_dir_path(root)
        if last.exists():
            build_dir = Path(last.read_text(encoding="utf-8").strip())
        else:
            raise ConfigError("No profile specified and no last build dir recorded")
    config = require_config(build_dir)
    return config, build_dir


def _record_last(build_root: Path, profile: str, build_dir: Path) -> None:
    write_text(last_profile_path(build_root), profile)
    write_text(last_build_dir_path(build_root), str(build_dir))


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


def _filter_source_files(files: list[Path]) -> list[Path]:
    exts = {".c", ".cc", ".cpp", ".cxx", ".h", ".hpp", ".hh", ".hxx", ".inl"}
    return [f for f in files if f.suffix.lower() in exts]


def _collect_files(paths: tuple[str, ...], repo: Path, allow_changed: bool, quiet: bool) -> list[Path]:
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


@click.group()
@click.option("--quiet", is_flag=True, help="Suppress command echo.")
@click.pass_context
def cli(ctx: click.Context, quiet: bool) -> None:
    ctx.obj = CliContext(quiet=quiet)


@cli.command()
@click.option("--profile", default="codex")
@click.option("--build-type", default="Debug")
@click.option("--build-root", type=click.Path(path_type=Path))
@click.option("--profile-host")
@click.option("--profile-build", default="default")
@click.option("--cc")
@click.option("--cxx")
@click.option("--cmake-define", multiple=True)
@click.option("--vulkan-sdk")
@click.option("--export-compile-commands/--no-export-compile-commands", default=True)
@click.option("--force", is_flag=True)
@click.pass_obj
def configure(
    ctx: CliContext,
    profile: str,
    build_type: str,
    build_root: Path | None,
    profile_host: str | None,
    profile_build: str,
    cc: str | None,
    cxx: str | None,
    cmake_define: tuple[str, ...],
    vulkan_sdk: str | None,
    export_compile_commands: bool,
    force: bool,
) -> None:
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
    defines = _parse_defines(cmake_define)
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
            "Vulkan_LIBRARY": str(Path(vulkan_sdk) / "lib" / ("vulkan-1.lib" if is_windows() else "libvulkan.so")),
        }
        defines.update(vulkan_hints)
    host_profile = profile_host or ("default" if is_windows() else "codex-clang")
    conan_mod.ensure_profile_detected(tools["conan"], ctx.quiet)
    conan_mod.install(
        tools["conan"],
        build_dir,
        repo_root(),
        host_profile,
        profile_build,
        build_type,
        ctx.quiet,
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
        vulkan_sdk=vulkan_sdk,
        vulkan_hints=vulkan_hints,
    )
    env = _config_env(config)
    cmake_mod.configure(
        tools["cmake"],
        build_dir,
        repo_root(),
        build_type,
        toolchain_file,
        defines,
        env,
        ctx.quiet,
    )
    write_config(build_dir, config)
    _record_last(root, profile, build_dir)


@cli.command()
@click.option("--profile")
@click.option("--conan", "run_conan", is_flag=True)
@click.option("--cmake-define", multiple=True)
@click.pass_obj
def reconfigure(ctx: CliContext, profile: str | None, run_conan: bool, cmake_define: tuple[str, ...]) -> None:
    config, build_dir = _load_or_fail(profile, None)
    env = _config_env(config)
    tools = config["tools"]
    if run_conan:
        conan_mod.install(
            tools["conan"],
            Path(config["paths"]["build_dir"]),
            Path(config["paths"]["source_dir"]),
            config["conan"]["profile_host"],
            config["conan"]["profile_build"],
            config["cbt"]["build_type"],
            ctx.quiet,
        )
    defines = dict(config["cmake"]["defines"])
    defines.update(_parse_defines(cmake_define))
    cmake_mod.configure(
        tools["cmake"],
        build_dir,
        Path(config["paths"]["source_dir"]),
        config["cbt"]["build_type"],
        Path(config["cmake"]["toolchain_file"]),
        defines,
        env,
        ctx.quiet,
    )


@cli.command()
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
    config, build_dir = _load_or_fail(profile, build_root)
    env = _config_env(config)
    cmake_mod.build(
        config["tools"]["cmake"],
        build_dir,
        config["cbt"]["build_type"],
        target,
        jobs,
        verbose,
        env,
        ctx.quiet,
    )


@cli.command(name="run")
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
    config, build_dir = _load_or_fail(profile, build_root)
    env = _config_env(config)
    if not no_build:
        try:
            cmake_mod.build(
                config["tools"]["cmake"],
                build_dir,
                config["cbt"]["build_type"],
                target,
                None,
                False,
                env,
                ctx.quiet,
            )
        except ToolError:
            pass
    exe = build_dir / "bin" / (target + (".exe" if is_windows() else ""))
    if not exe.exists():
        raise ConfigError(f"Executable not found: {exe}")
    if working_dir:
        os.chdir(working_dir)
    run_mod.run_target(exe, list(args), env, ctx.quiet)


@cli.command(name="targets")
@click.option("--profile")
@click.option("--build-root", type=click.Path(path_type=Path))
@click.option("--json", "as_json", is_flag=True)
@click.pass_obj
def list_targets(ctx: CliContext, profile: str | None, build_root: Path | None, as_json: bool) -> None:
    config, build_dir = _load_or_fail(profile, build_root)
    env = _config_env(config)
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


@cli.command(name="tests")
@click.option("--profile")
@click.option("--build-root", type=click.Path(path_type=Path))
@click.option("--json", "as_json", is_flag=True)
@click.pass_obj
def list_tests(ctx: CliContext, profile: str | None, build_root: Path | None, as_json: bool) -> None:
    config, build_dir = _load_or_fail(profile, build_root)
    env = _config_env(config)
    tests = ctest_mod.list_tests(config["tools"]["ctest"], build_dir, env, ctx.quiet)
    if as_json:
        print_json({"tests": tests})
    else:
        for t in tests:
            sys.stdout.write(f"{t}\n")


@cli.command(name="test")
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
    config, build_dir = _load_or_fail(profile, build_root)
    env = _test_env(config)
    target_help = run(
        [config["tools"]["cmake"], "--build", str(build_dir), "--target", "help"],
        env=env,
        quiet=ctx.quiet,
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
                ctx.quiet,
            )
            break
    else:
        cmake_mod.build(
            config["tools"]["cmake"],
            build_dir,
            config["cbt"]["build_type"],
            None,
            None,
            False,
            env,
            ctx.quiet,
        )
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


@cli.command()
@click.option("--profile")
@click.option("--build-root", type=click.Path(path_type=Path))
@click.argument("sources", nargs=-1, required=True)
@click.pass_obj
def compile(ctx: CliContext, profile: str | None, build_root: Path | None, sources: tuple[str, ...]) -> None:
    config, build_dir = _load_or_fail(profile, build_root)
    compile_mod.compile_sources(build_dir, [Path(s) for s in sources], ctx.quiet)


@cli.command()
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
    config, build_dir = _load_or_fail(profile, build_root)
    analyze_mod.analyze_files(
        config["tools"]["clang_tidy"],
        build_dir,
        [Path(s) for s in sources],
        checks,
        fix,
        ctx.quiet,
    )


@cli.command()
@click.option("--profile")
@click.option("--build-root", type=click.Path(path_type=Path))
@click.option("--check", is_flag=True)
@click.argument("paths", nargs=-1)
@click.pass_obj
def fmt(ctx: CliContext, profile: str | None, build_root: Path | None, check: bool, paths: tuple[str, ...]) -> None:
    config, _ = _load_or_fail(profile, build_root)
    repo = repo_root()
    files = _filter_source_files(_collect_files(paths, repo, True, ctx.quiet))
    if not files:
        raise ConfigError("No files to format")
    format_mod.format_files(config["tools"]["clang_format"], files, check, ctx.quiet)


@cli.command()
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


@cli.command()
@click.option("--profile")
@click.option("--build-root", type=click.Path(path_type=Path))
@click.option("--yes", is_flag=True)
@click.option("--confirm")
@click.pass_obj
def clean(ctx: CliContext, profile: str | None, build_root: Path | None, yes: bool, confirm: str | None) -> None:
    root = build_root or default_build_root()
    if not profile:
        raise ConfigError("Profile required for clean")
    if not yes and confirm != profile:
        raise ConfigError("Confirm with --yes or --confirm <profile>")
    build_dir = build_dir_for_profile(profile, root)
    if build_dir.exists():
        shutil.rmtree(build_dir)


@cli.group()
def cache() -> None:
    pass


@cache.command("show")
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


@cache.command("prune")
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


@cache.command("conan")
@click.option("--yes", is_flag=True)
@click.pass_obj
def cache_conan(ctx: CliContext, yes: bool) -> None:
    if not yes:
        raise ConfigError("Confirm with --yes")
    conan = which("conan")
    if not conan:
        raise MissingPrereq("conan not found")
    run([conan, "cache", "clean", "--all"], quiet=ctx.quiet)


@cli.command()
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


@cli.command()
@click.option("--profile")
@click.option("--build-root", type=click.Path(path_type=Path))
@click.option("--json", "as_json", is_flag=True)
@click.pass_obj
def status(ctx: CliContext, profile: str | None, build_root: Path | None, as_json: bool) -> None:
    config, build_dir = _load_or_fail(profile, build_root)
    info = {
        "repo_root": config["paths"]["source_dir"],
        "build_root": config["paths"]["build_root"],
        "build_dir": str(build_dir),
        "build_type": config["cbt"]["build_type"],
        "tools": config.get("tools", {}),
        "conan": config.get("conan", {}),
        "compile_commands": str(Path(build_dir) / "compile_commands.json"),
    }
    if as_json:
        print_json(info)
    else:
        for k, v in info.items():
            sys.stdout.write(f"{k}: {v}\n")


@cli.command(name="which")
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


@cli.command()
@click.option("--profile")
@click.option("--build-root", type=click.Path(path_type=Path))
@click.pass_obj
def env(ctx: CliContext, profile: str | None, build_root: Path | None) -> None:
    config, _ = _load_or_fail(profile, build_root)
    env = _config_env(config)
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
