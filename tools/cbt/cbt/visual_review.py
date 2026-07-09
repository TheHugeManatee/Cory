from __future__ import annotations

import json
import re
import struct
import subprocess
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Iterable

from . import git as git_mod
from .util import ConfigError, ensure_dir, run

_IMAGE_EXTENSIONS = {".bmp"}
_REQUEST_SCHEMA = "cory.visual-review-request.v1"
_MANIFEST_SCHEMA = "cory.cbt-visual-review-manifest.v1"
_TEST_SOURCE_SUFFIXES = {".cpp", ".cc", ".cxx", ".hpp", ".hh", ".hxx", ".inl"}
_TEST_SOURCE_ROOTS = ("tests", "tools", "test-support", "examples")
_TEST_CASE_PATTERN = re.compile(
    r'TEST_CASE(?:_METHOD)?\s*\(\s*"(?P<name>(?:\\.|[^"])*)"\s*,\s*"(?P<tags>(?:\\.|[^"])*)"',
    re.DOTALL,
)


@dataclass(frozen=True)
class RgbaImage:
    width: int
    height: int
    pixels: bytes


@dataclass(frozen=True)
class ImageMetrics:
    mismatched_pixels: int
    mismatch_ratio: float
    max_channel_error: int
    mean_absolute_error: float


@dataclass(frozen=True)
class ReviewManifest:
    schema: str
    mode: str
    source_path: str
    reference_ref: str
    source_exists: bool
    reference_exists: bool
    request_path: str
    decision_path: str


@dataclass(frozen=True)
class ReviewRequest:
    request_path: Path
    decision_path: Path
    manifest_path: Path | None
    label: str


@dataclass(frozen=True)
class DiscoveredRequest:
    request_path: Path
    label: str


@dataclass(frozen=True)
class WorktreeReviewContext:
    source_path: Path
    request_dir: Path
    request_path: Path
    decision_path: Path
    manifest_path: Path
    reference_ref: str
    source_exists: bool
    reference_exists: bool


def review_visual_changes(
    *,
    repo_root: Path,
    build_root: Path,
    reviewer_executable: Path,
    env: dict[str, str],
    quiet: bool,
    upstream: bool,
    scan: bool,
) -> None:
    if scan:
        requests = discover_open_requests(build_root)
        if not requests:
            if not quiet:
                print(f"No open visual review requests found under {build_root}")
            return
        if not quiet:
            _print_request_summary("Open review requests", [request.label for request in requests])
        for request in requests:
            _review_existing_request(request, reviewer_executable, env, quiet)
        return

    reference_ref = "develop" if upstream else "HEAD"
    source_paths = discover_worktree_image_changes(repo_root, reference_ref, quiet)
    if not source_paths:
        if not quiet:
            print(f"No changed golden images found against {reference_ref}")
        return

    if not quiet:
        _print_request_summary(
            f"Golden images changed against {reference_ref}",
            [str(path.resolve()) for path in source_paths],
        )

    session_root = build_root / "visual-review" / ("upstream" if upstream else "worktree")
    for source_path in source_paths:
        context = create_worktree_review_request(
            repo_root=repo_root,
            source_path=source_path,
            reference_ref=reference_ref,
            session_root=session_root,
        )
        if not quiet:
            print(f"Opening visual review: {source_path.resolve()}")
        decision = launch_reviewer(reviewer_executable, context.request_path, env, quiet)
        if not decision.get("accepted", False):
            restore_source_to_reference(repo_root, context.source_path, context.reference_ref)
            if not quiet:
                print(f"Reverted {context.source_path.resolve()} to {context.reference_ref}")
        elif not quiet:
            print(f"Accepted {context.source_path.resolve()}")


def discover_worktree_image_changes(repo_root: Path, reference_ref: str, quiet: bool) -> list[Path]:
    if reference_ref == "HEAD":
        changed = git_mod.changed_files_worktree(repo_root, quiet)
    else:
        changed = git_mod.changed_files_against_with_untracked(repo_root, reference_ref, quiet)
    paths = _normalize_changed_paths(repo_root, changed)
    return [path for path in paths if path.suffix.lower() in _IMAGE_EXTENSIONS]


def discover_visual_tests(repo_root: Path) -> list[str]:
    discovered: list[str] = []
    seen: set[str] = set()
    for root_name in _TEST_SOURCE_ROOTS:
        root = repo_root / root_name
        if not root.exists():
            continue
        for file_path in root.rglob("*"):
            if file_path.suffix.lower() not in _TEST_SOURCE_SUFFIXES:
                continue
            try:
                text = file_path.read_text(encoding="utf-8")
            except Exception:
                continue
            for match in _TEST_CASE_PATTERN.finditer(text):
                tags = match.group("tags")
                if "[visual]" not in tags:
                    continue
                test_name = f"unittests.{match.group('name')}"
                if test_name in seen:
                    continue
                seen.add(test_name)
                discovered.append(test_name)
    return discovered


def discover_open_requests(build_root: Path) -> list[DiscoveredRequest]:
    discovered: list[DiscoveredRequest] = []
    if not build_root.exists():
        return discovered

    for request_path in sorted(build_root.rglob("request.json")):
        request = read_request(request_path)
        if request is None:
            continue
        decision_path = resolve_path(request_path.parent, str(request.get("decisionPath", "")))
        if decision_path.exists():
            continue
        metadata = request.get("metadata") if isinstance(request.get("metadata"), dict) else {}
        label = str(metadata.get("sourceFile") or request.get("caseName") or request_path.parent.name)
        discovered.append(DiscoveredRequest(request_path=request_path, label=label))
    return discovered


def create_worktree_review_request(
    *,
    repo_root: Path,
    source_path: Path,
    reference_ref: str,
    session_root: Path,
) -> WorktreeReviewContext:
    source_path = source_path.resolve()
    ensure_dir(session_root)

    relative_name = _safe_slug(_relative_display_path(repo_root, source_path))
    timestamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    request_dir = session_root / relative_name / timestamp
    ensure_dir(request_dir)

    source_bytes = source_path.read_bytes() if source_path.exists() else None
    reference_bytes = read_git_blob(repo_root, reference_ref, source_path)

    source_image = read_bmp(source_bytes) if source_bytes is not None else None
    reference_image = read_bmp(reference_bytes) if reference_bytes is not None else None

    if source_image is None and reference_image is None:
        raise ConfigError(f"Cannot review {source_path}: neither source nor reference image is available")

    if source_image is None and reference_image is not None:
        source_image = blank_like(reference_image)
    if reference_image is None and source_image is not None:
        reference_image = blank_like(source_image)

    assert source_image is not None
    assert reference_image is not None

    normalized_reference, normalized_source = normalize_pair(reference_image, source_image)
    diff_image = make_diff_image(normalized_reference, normalized_source)
    metrics = compare_images(normalized_reference, normalized_source)

    baseline_path = request_dir / "baseline.bmp"
    actual_path = request_dir / "actual.bmp"
    diff_path = request_dir / "diff.bmp"
    metrics_path = request_dir / "metrics.json"
    request_path = request_dir / "request.json"
    decision_path = request_dir / "decision.json"
    manifest_path = request_dir / "manifest.json"

    write_bmp(baseline_path, normalized_reference)
    write_bmp(actual_path, normalized_source)
    write_bmp(diff_path, diff_image)
    write_text(metrics_path, json.dumps(_metrics_json(source_path, metrics, request_dir), indent=2) + "\n")

    request = {
        "schema": _REQUEST_SCHEMA,
        "id": _request_id(source_path, reference_ref),
        "caseName": _relative_display_path(repo_root, source_path),
        "metadata": {
            "catchTestName": "cbt visual-review",
            "sourceFile": str(source_path),
            "sourceLine": 1,
            "sourceFunction": f"reference={reference_ref}",
        },
        "baselinePath": str(baseline_path),
        "actualPath": str(actual_path),
        "diffPath": str(diff_path),
        "metricsPath": str(metrics_path),
        "requestPath": str(request_path),
        "decisionPath": str(decision_path),
        "metrics": {
            "mismatchedPixels": metrics.mismatched_pixels,
            "mismatchRatio": metrics.mismatch_ratio,
            "maxChannelError": metrics.max_channel_error,
            "meanAbsoluteError": metrics.mean_absolute_error,
        },
    }
    write_text(request_path, json.dumps(request, indent=2) + "\n")

    manifest = ReviewManifest(
        schema=_MANIFEST_SCHEMA,
        mode="worktree",
        source_path=str(source_path),
        reference_ref=reference_ref,
        source_exists=source_path.exists(),
        reference_exists=reference_bytes is not None,
        request_path=str(request_path),
        decision_path=str(decision_path),
    )
    write_text(manifest_path, json.dumps(manifest.__dict__, indent=2) + "\n")

    return WorktreeReviewContext(
        source_path=source_path,
        request_dir=request_dir,
        request_path=request_path,
        decision_path=decision_path,
        manifest_path=manifest_path,
        reference_ref=reference_ref,
        source_exists=source_path.exists(),
        reference_exists=reference_bytes is not None,
    )


def _review_existing_request(request: DiscoveredRequest, reviewer_executable: Path, env: dict[str, str], quiet: bool) -> None:
    if not quiet:
        print(f"Opening visual review: {request.label}")
    decision = launch_reviewer(reviewer_executable, request.request_path, env, quiet)
    accepted = bool(decision.get("accepted", False))
    note = str(decision.get("note", ""))
    if not quiet:
        outcome = "accepted" if accepted else "rejected"
        print(f"Review closed: {outcome} ({request.label}){f' - {note}' if note else ''}")


def launch_reviewer(reviewer_executable: Path, request_path: Path, env: dict[str, str], quiet: bool) -> dict[str, object]:
    run([str(reviewer_executable), "--request", str(request_path)], env=env, quiet=quiet)
    request = read_request(request_path)
    if request is None:
        raise ConfigError(f"Invalid visual review request: {request_path}")
    decision = read_decision(resolve_path(request_path.parent, str(request["decisionPath"])))
    if decision is None:
        raise ConfigError(f"Visual reviewer did not write a decision file: {request_path}")
    if str(decision.get("requestId", "")) != str(request.get("id", "")):
        raise ConfigError(
            f"Visual reviewer decision id mismatch for {request_path}: "
            f"expected {request.get('id', '')}, got {decision.get('requestId', '')}"
        )
    return decision


def restore_source_to_reference(repo_root: Path, source_path: Path, reference_ref: str) -> None:
    relative = str(source_path.relative_to(repo_root))
    tracked = subprocess.run(
        ["git", "ls-files", "--error-unmatch", "--", relative],
        cwd=str(repo_root),
        capture_output=True,
        text=True,
    ).returncode == 0

    cmd = [
        "git",
        "restore",
        "--source",
        reference_ref,
        "--staged",
        "--worktree",
        "--",
        relative,
    ]
    result = subprocess.run(cmd, cwd=str(repo_root), capture_output=True, text=True)
    if result.returncode == 0:
        return

    if not tracked:
        if source_path.exists():
            source_path.unlink()
        return

    raise ConfigError(
        f"Failed to restore {source_path} to {reference_ref}: {result.stderr.strip() or 'unknown error'}"
    )


def read_request(request_path: Path) -> dict[str, object] | None:
    try:
        raw = request_path.read_text(encoding="utf-8")
        request = json.loads(raw)
        if request.get("schema") != _REQUEST_SCHEMA:
            return None
        if not request.get("id") or not request.get("decisionPath"):
            return None
        return request
    except Exception:
        return None


def read_decision(decision_path: Path) -> dict[str, object] | None:
    try:
        return json.loads(decision_path.read_text(encoding="utf-8"))
    except Exception:
        return None


def read_git_blob(repo_root: Path, reference_ref: str, source_path: Path) -> bytes | None:
    relative = source_path.relative_to(repo_root).as_posix()
    cmd = ["git", "show", f"{reference_ref}:{relative}"]
    result = subprocess.run(cmd, cwd=str(repo_root), capture_output=True)
    if result.returncode != 0:
        return None
    return result.stdout


def read_bmp(raw: bytes | None) -> RgbaImage | None:
    if raw is None or len(raw) < 54:
        return None
    if raw[0:2] != b"BM":
        return None

    pixel_offset = struct.unpack_from("<I", raw, 10)[0]
    header_size = struct.unpack_from("<I", raw, 14)[0]
    if header_size < 40:
        return None

    width = struct.unpack_from("<i", raw, 18)[0]
    height_signed = struct.unpack_from("<i", raw, 22)[0]
    planes = struct.unpack_from("<H", raw, 26)[0]
    bpp = struct.unpack_from("<H", raw, 28)[0]
    compression = struct.unpack_from("<I", raw, 30)[0]
    if planes != 1 or bpp != 32 or compression != 0 or width <= 0 or height_signed == 0:
        return None

    height = abs(height_signed)
    row_size = width * 4
    expected = pixel_offset + row_size * height
    if len(raw) < expected:
        return None

    pixels = bytearray(width * height * 4)
    for y in range(height):
        src_y = y if height_signed < 0 else height - 1 - y
        src_offset = pixel_offset + src_y * row_size
        dst_offset = y * row_size
        row = raw[src_offset : src_offset + row_size]
        for x in range(0, row_size, 4):
            b, g, r, a = row[x : x + 4]
            pixels[dst_offset + x : dst_offset + x + 4] = bytes((r, g, b, a))
    return RgbaImage(width=width, height=height, pixels=bytes(pixels))


def write_bmp(path: Path, image: RgbaImage) -> None:
    ensure_dir(path.parent)
    row_size = image.width * 4
    pixel_size = row_size * image.height
    file_size = 54 + pixel_size

    header = bytearray()
    header.extend(b"BM")
    header.extend(struct.pack("<IHHI", file_size, 0, 0, 54))
    header.extend(struct.pack("<IiiHHIIiiII", 40, image.width, image.height, 1, 32, 0, pixel_size, 2835, 2835, 0, 0))

    data = bytearray()
    for y in range(image.height - 1, -1, -1):
        row = image.pixels[y * row_size : (y + 1) * row_size]
        for x in range(0, row_size, 4):
            r, g, b, a = row[x : x + 4]
            data.extend((b, g, r, a))

    path.write_bytes(bytes(header) + bytes(data))


def make_diff_image(reference: RgbaImage, actual: RgbaImage) -> RgbaImage:
    reference, actual = normalize_pair(reference, actual)
    pixels = bytearray(len(actual.pixels))
    for i in range(0, len(actual.pixels), 4):
        ar, ag, ab = actual.pixels[i : i + 3]
        br, bg, bb = reference.pixels[i : i + 3]
        pixels[i + 0] = min(255, abs(ar - br) * 8)
        pixels[i + 1] = min(255, abs(ag - bg) * 8)
        pixels[i + 2] = min(255, abs(ab - bb) * 8)
        pixels[i + 3] = 255
    return RgbaImage(width=actual.width, height=actual.height, pixels=bytes(pixels))


def compare_images(reference: RgbaImage, actual: RgbaImage) -> ImageMetrics:
    reference, actual = normalize_pair(reference, actual)
    mismatched_pixels = 0
    total_error = 0
    max_channel_error = 0
    for i in range(0, len(actual.pixels), 4):
        pixel_mismatch = False
        for c in range(4):
            error = abs(actual.pixels[i + c] - reference.pixels[i + c])
            total_error += error
            max_channel_error = max(max_channel_error, error)
            if error > 0:
                pixel_mismatch = True
        if pixel_mismatch:
            mismatched_pixels += 1
    pixel_count = reference.width * reference.height
    mismatch_ratio = 0.0 if pixel_count == 0 else mismatched_pixels / float(pixel_count)
    mean_absolute_error = 0.0 if not actual.pixels else total_error / float(len(actual.pixels))
    return ImageMetrics(
        mismatched_pixels=mismatched_pixels,
        mismatch_ratio=mismatch_ratio,
        max_channel_error=max_channel_error,
        mean_absolute_error=mean_absolute_error,
    )


def normalize_pair(reference: RgbaImage, actual: RgbaImage) -> tuple[RgbaImage, RgbaImage]:
    width = max(reference.width, actual.width)
    height = max(reference.height, actual.height)
    return pad_image(reference, width, height), pad_image(actual, width, height)


def pad_image(image: RgbaImage, width: int, height: int) -> RgbaImage:
    pixels = bytearray(width * height * 4)
    src_row_size = image.width * 4
    dst_row_size = width * 4
    for y in range(image.height):
        src_offset = y * src_row_size
        dst_offset = y * dst_row_size
        row = image.pixels[src_offset : src_offset + src_row_size]
        pixels[dst_offset : dst_offset + src_row_size] = row
    return RgbaImage(width=width, height=height, pixels=bytes(pixels))


def blank_like(image: RgbaImage) -> RgbaImage:
    return RgbaImage(width=image.width, height=image.height, pixels=bytes(image.width * image.height * 4))


def resolve_path(base: Path, candidate: str) -> Path:
    candidate_path = Path(candidate)
    if candidate_path.is_absolute():
        return candidate_path
    return (base / candidate_path).resolve()


def write_text(path: Path, content: str) -> None:
    ensure_dir(path.parent)
    path.write_text(content, encoding="utf-8")


def _metrics_json(source_path: Path, metrics: ImageMetrics, request_dir: Path) -> dict[str, object]:
    return {
        "case": str(source_path.resolve()),
        "passed": False,
        "mismatchedPixels": metrics.mismatched_pixels,
        "mismatchRatio": metrics.mismatch_ratio,
        "maxChannelError": metrics.max_channel_error,
        "meanAbsoluteError": metrics.mean_absolute_error,
        "artifactDir": str(request_dir),
    }


def _request_id(source_path: Path, reference_ref: str) -> str:
    timestamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    return f"cbt-visual-review-{_safe_slug(source_path.name)}-{reference_ref}-{timestamp}"


def _safe_slug(value: str) -> str:
    return re.sub(r"[^A-Za-z0-9._-]+", "-", value).strip("-_") or "visual-review"


def _relative_display_path(repo_root: Path, path: Path) -> str:
    try:
        return path.resolve().relative_to(repo_root.resolve()).as_posix()
    except Exception:
        return str(path.resolve())


def _normalize_changed_paths(repo_root: Path, changed: Iterable[str]) -> list[Path]:
    result: list[Path] = []
    seen: set[Path] = set()
    for entry in changed:
        path = (repo_root / entry).resolve()
        if path in seen:
            continue
        seen.add(path)
        result.append(path)
    return sorted(result)


def visual_test_regex(test_names: list[str]) -> str:
    escaped = [escape_regex_literal(test_name) for test_name in test_names]
    return rf"^({'|'.join(escaped)})$"


def escape_regex_literal(value: str) -> str:
    return re.sub(r"([\\.^$|?*+()[\]{}])", r"\\\1", value)


def _print_request_summary(title: str, items: list[str]) -> None:
    print(f"{title} ({len(items)}):")
    for item in items:
        print(f"  - {item}")
