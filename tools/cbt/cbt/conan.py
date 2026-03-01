from __future__ import annotations

import json
from pathlib import Path
import re
import shutil

from .util import ConfigError, ensure_dir, read_text, run, write_text


def _default_conan_home() -> Path:
    return Path.home() / ".conan2"


def _profiles_dir(home: Path) -> Path:
    return home / "profiles"


def _sync_settings_file(conan_home: Path, quiet: bool) -> None:
    source = _default_conan_home() / "settings.yml"
    if not source.exists():
        return

    dest = conan_home / "settings.yml"
    ensure_dir(conan_home)
    shutil.copy2(source, dest)
    if not quiet:
        print(f"Synchronized Conan settings from {source} to {dest}")


def _ensure_requested_profiles(
    conan_home: Path, requested_profiles: list[str], quiet: bool
) -> None:
    source_profiles = _profiles_dir(_default_conan_home())
    dest_profiles = _profiles_dir(conan_home)
    ensure_dir(dest_profiles)

    for profile in requested_profiles:
        if not profile:
            continue

        dest = dest_profiles / profile
        if dest.exists() or profile == "default":
            continue

        source = source_profiles / profile
        if source.exists():
            shutil.copy2(source, dest)
            if not quiet:
                print(f"Copied Conan profile '{profile}' into {dest}")
            continue

        raise ConfigError(
            f"Conan profile '{profile}' was not found in {dest_profiles} or {source_profiles}."
        )


def ensure_profile_detected(
    conan: str,
    quiet: bool,
    env: dict[str, str] | None = None,
    requested_profiles: list[str] | None = None,
) -> None:
    conan_home = Path((env or {}).get("CONAN_HOME", str(_default_conan_home())))
    ensure_dir(conan_home)
    _sync_settings_file(conan_home, quiet)
    if requested_profiles:
        _ensure_requested_profiles(conan_home, requested_profiles, quiet)
    try:
        # If the default profile exists this will succeed; don't run detect.
        run([conan, "profile", "show", "default"], env=env, quiet=True)
        return
    except Exception:
        # No default profile: auto-detect one and then ensure cppstd=20
        run([conan, "profile", "detect", "--force"], env=env, quiet=quiet)
        # Conan 2 removed `profile update` - edit the profile file directly.
        try:
            # `conan profile path default` prints the path to the profile file.
            res = run([conan, "profile", "path", "default"], env=env, quiet=True)
            profile_path_str = res.stdout.strip()
            if not profile_path_str:
                return
            profile_path = Path(profile_path_str)
            if not profile_path.exists():
                return

            content = read_text(profile_path)
            lines = content.splitlines(keepends=True)

            # Find [settings] section
            settings_idx = None
            for i, line in enumerate(lines):
                if line.strip().lower() == "[settings]":
                    settings_idx = i
                    break

            cppstd_pattern = re.compile(r'^\s*(?:settings\.)?compiler\.cppstd\s*=\s*', re.IGNORECASE)

            if settings_idx is not None:
                # find end of [settings] (next section) and look for existing cppstd
                end_idx = len(lines)
                for j in range(settings_idx + 1, len(lines)):
                    if lines[j].strip().startswith("["):
                        end_idx = j
                        break

                found = False
                for k in range(settings_idx + 1, end_idx):
                    if cppstd_pattern.match(lines[k]):
                        # replace the line with the desired setting
                        lines[k] = re.sub(r'^(.*=).*', r"compiler.cppstd=20\n", lines[k])
                        found = True
                        break

                if not found:
                    # insert after the [settings] header
                    insert_at = settings_idx + 1
                    # keep consistent newline style
                    nl = "\n" if not lines[settings_idx].endswith("\n") else ""
                    lines.insert(insert_at, "compiler.cppstd=20\n")
            else:
                # No [settings] section - append one
                if lines and not lines[-1].endswith("\n"):
                    lines[-1] = lines[-1] + "\n"
                lines.append("[settings]\n")
                lines.append("compiler.cppstd=20\n")

            new_content = "".join(lines)
            if new_content != content:
                write_text(profile_path, new_content)
        except Exception:
            # On any failure here we don't want to crash the whole setup; it's best-effort.
            return


def install(
    conan: str,
    build_dir: Path,
    source_dir: Path,
    profile_host: str,
    profile_build: str,
    build_type: str,
    quiet: bool,
    preset_name: str | None = None,
    env: dict[str, str] | None = None,
    conf: dict[str, list[str]] | None = None,
) -> None:
    cmd = [
        conan,
        "install",
        str(source_dir),
        "--output-folder",
        str(build_dir),
        "--build=missing",
        "-s",
        f"build_type={build_type}",
        "-pr:h",
        profile_host,
        "-pr:b",
        profile_build,
    ]
    for key, value in (conf or {}).items():
        cmd.extend(["-c", f"{key}={json.dumps(value)}"])
    run(cmd, env=env, quiet=quiet)
    # If Conan or CMake emitted a CMakePresets.json in the build dir, patch its
    # configure/build/test preset names so that they match the `cbt` profile name
    # (preset_name). This ensures the generated presets are aligned with the
    # cbt profile, which helps editors and automation that consume the presets.
    if preset_name:
        try:
            presets_path = build_dir / "CMakePresets.json"
            if presets_path.exists():
                try:
                    data = json.loads(presets_path.read_text(encoding="utf-8"))
                except Exception:
                    data = None
                if isinstance(data, dict):
                    # Update top-level configure preset name (first entry)
                    cfgs = data.get("configurePresets")
                    if isinstance(cfgs, list) and len(cfgs) > 0:
                        cfgs[0]["name"] = preset_name
                    # Ensure buildPresets and testPresets reference the new configurePreset
                    for list_key in ("buildPresets", "testPresets"):
                        items = data.get(list_key)
                        if isinstance(items, list):
                            for it in items:
                                # set configurePreset and normalize the preset name
                                it["configurePreset"] = preset_name
                                # also set the preset's own name to be the profile for simplicity
                                it["name"] = preset_name
                    # Write the patched presets back
                    presets_path.write_text(json.dumps(data, indent=4, sort_keys=False), encoding="utf-8")
        except Exception:
            # Do not fail the install if patching fails; log could be added later.
            pass
