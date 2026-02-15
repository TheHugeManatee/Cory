from __future__ import annotations

from pathlib import Path
import re
import json

from .util import run, write_text, read_text

def ensure_profile_detected(conan: str, quiet: bool) -> None:
    try:
        # If the default profile exists this will succeed; don't run detect.
        run([conan, "profile", "show", "default"], quiet=True)
        return
    except Exception:
        # No default profile: auto-detect one and then ensure cppstd=20
        run([conan, "profile", "detect", "--force"], quiet=quiet)
        # Conan 2 removed `profile update` - edit the profile file directly.
        try:
            # `conan profile path default` prints the path to the profile file.
            res = run([conan, "profile", "path", "default"], quiet=True)
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
    run(cmd, quiet=quiet)
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
