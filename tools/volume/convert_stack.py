#!/usr/bin/env python3
from __future__ import annotations

import argparse
import math
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Sequence

import numpy as np
from PIL import Image


@dataclass(frozen=True)
class VolumeDims:
    x: int
    y: int
    z: int

    @property
    def voxel_count(self) -> int:
        return self.x * self.y * self.z


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Convert an image stack into Cory .cvol format (preview + full raw blobs)."
    )
    parser.add_argument("--input-dir", type=Path, required=True, help="Directory containing slice images")
    parser.add_argument("--output-dir", type=Path, required=True, help="Output directory")
    parser.add_argument("--dataset-id", required=True, help="Dataset id used in .cvol manifest")
    parser.add_argument(
        "--pattern",
        default="*.bmp",
        help="Glob pattern for slices (default: *.bmp)",
    )
    parser.add_argument(
        "--regex",
        default=None,
        help="Optional regex filter for file names after glob expansion",
    )
    parser.add_argument(
        "--spacing-mm",
        nargs=3,
        type=float,
        metavar=("SX", "SY", "SZ"),
        default=(1.0, 1.0, 1.0),
        help="Voxel spacing in millimeters (default: 1 1 1)",
    )
    parser.add_argument(
        "--preview-max-dim",
        type=int,
        default=192,
        help="Max dimension for preview volume (default: 192)",
    )
    parser.add_argument(
        "--full-max-dim",
        type=int,
        default=1024,
        help="Maximum allowed dimension for full volume before downsampling (default: 1024)",
    )
    parser.add_argument(
        "--full-max-bytes",
        type=int,
        default=1_500_000_000,
        help="Maximum allowed bytes for full volume before downsampling (default: 1500000000)",
    )
    parser.add_argument(
        "--pct-low",
        type=float,
        default=0.5,
        help="Low percentile for normalization (default: 0.5)",
    )
    parser.add_argument(
        "--pct-high",
        type=float,
        default=99.5,
        help="High percentile for normalization (default: 99.5)",
    )
    parser.add_argument(
        "--force-normalize",
        action="store_true",
        help="Normalize even if source slices are 8-bit",
    )
    parser.add_argument(
        "--append-catalog",
        type=Path,
        default=None,
        help="Optional .cvolcat file to append dataset entry to",
    )
    return parser.parse_args()


def discover_slices(input_dir: Path, pattern: str, regex: str | None) -> list[Path]:
    if not input_dir.exists() or not input_dir.is_dir():
        raise RuntimeError(f"Input directory does not exist: {input_dir}")

    candidates = sorted(input_dir.glob(pattern))
    if regex:
        re_obj = re.compile(regex)
        candidates = [p for p in candidates if re_obj.search(p.name)]

    indexed: list[tuple[int, Path]] = []
    for path in candidates:
        if not path.is_file():
            continue
        match = re.search(r"(\d+)(?!.*\d)", path.stem)
        if not match:
            continue
        indexed.append((int(match.group(1)), path))

    if not indexed:
        raise RuntimeError(
            f"No slices discovered in {input_dir} with pattern='{pattern}'"
            + (f", regex='{regex}'" if regex else "")
        )

    indexed.sort(key=lambda x: x[0])
    indices = [i for i, _ in indexed]
    for prev, curr in zip(indices, indices[1:]):
        if curr != prev + 1:
            raise RuntimeError(f"Slice index gap detected: {prev} -> {curr}")

    return [p for _, p in indexed]


def load_slice(path: Path) -> np.ndarray:
    with Image.open(path) as image:
        mode = image.mode
        if mode in ("L", "P"):
            image = image.convert("L")
            arr = np.asarray(image, dtype=np.uint8)
        elif mode in ("I;16", "I"):
            arr = np.asarray(image)
            if arr.dtype == np.int32:
                arr = np.clip(arr, 0, np.iinfo(np.uint16).max).astype(np.uint16, copy=False)
            elif arr.dtype != np.uint16:
                arr = arr.astype(np.uint16, copy=False)
        else:
            raise RuntimeError(f"Unsupported slice mode '{mode}' in {path}")

    if arr.ndim != 2:
        raise RuntimeError(f"Expected single-channel 2D slice, got shape {arr.shape} in {path}")
    return arr


def validate_stack(slices: Sequence[Path]) -> tuple[tuple[int, int], np.dtype]:
    first = load_slice(slices[0])
    shape = first.shape
    dtype = first.dtype
    for path in slices[1:]:
        arr = load_slice(path)
        if arr.shape != shape:
            raise RuntimeError(f"Mismatched slice dimensions: {path} has {arr.shape}, expected {shape}")
        if arr.dtype != dtype:
            raise RuntimeError(f"Mismatched slice dtype: {path} has {arr.dtype}, expected {dtype}")
    return (shape[1], shape[0]), dtype


def percentile_from_histogram(hist: np.ndarray, low_pct: float, high_pct: float) -> tuple[float, float]:
    cdf = np.cumsum(hist, dtype=np.uint64)
    total = int(cdf[-1])
    if total == 0:
        return 0.0, 255.0

    low_target = total * (low_pct / 100.0)
    high_target = total * (high_pct / 100.0)
    low = int(np.searchsorted(cdf, low_target, side="left"))
    high = int(np.searchsorted(cdf, high_target, side="left"))
    if high <= low:
        high = low + 1
    return float(low), float(high)


def compute_normalization_window(
    slices: Sequence[Path], dtype: np.dtype, low_pct: float, high_pct: float, force_normalize: bool
) -> tuple[float, float, str]:
    if dtype == np.uint8 and not force_normalize:
        return 0.0, 255.0, "identity"

    if dtype == np.uint16:
        hist = np.zeros(65536, dtype=np.uint64)
        for i, path in enumerate(slices):
            arr = load_slice(path)
            hist += np.bincount(arr.ravel(), minlength=65536)
            if (i + 1) % 200 == 0:
                print(f"[normalize] histogram pass {i + 1}/{len(slices)}")
        low, high = percentile_from_histogram(hist, low_pct, high_pct)
        return low, high, f"percentile:{low_pct},{high_pct}"

    mins: list[float] = []
    maxs: list[float] = []
    for i, path in enumerate(slices):
        arr = load_slice(path).astype(np.float32, copy=False)
        mins.append(float(arr.min()))
        maxs.append(float(arr.max()))
        if (i + 1) % 200 == 0:
            print(f"[normalize] min/max pass {i + 1}/{len(slices)}")
    low = float(min(mins))
    high = float(max(maxs))
    if high <= low:
        high = low + 1.0
    return low, high, f"minmax:{low},{high}"


def normalize_to_u8(arr: np.ndarray, low: float, high: float) -> np.ndarray:
    if arr.dtype == np.uint8 and low <= 0.0 and high >= 255.0:
        return arr
    arr_f = arr.astype(np.float32, copy=False)
    scaled = np.clip((arr_f - low) / (high - low), 0.0, 1.0)
    return (scaled * 255.0 + 0.5).astype(np.uint8)


def scaled_dims(src: VolumeDims, scale: float) -> VolumeDims:
    return VolumeDims(
        x=max(1, int(round(src.x * scale))),
        y=max(1, int(round(src.y * scale))),
        z=max(1, int(round(src.z * scale))),
    )


def compute_preview_dims(src: VolumeDims, max_dim: int) -> VolumeDims:
    scale = min(1.0, float(max_dim) / float(max(src.x, src.y, src.z)))
    return scaled_dims(src, scale)


def compute_full_dims(src: VolumeDims, max_dim: int, max_bytes: int) -> tuple[VolumeDims, bool, str]:
    scale = 1.0
    reason = ""
    if max(src.x, src.y, src.z) > max_dim:
        scale = min(scale, float(max_dim) / float(max(src.x, src.y, src.z)))
        reason = f"max_dim={max_dim}"

    src_bytes = src.voxel_count
    if src_bytes > max_bytes:
        byte_scale = (float(max_bytes) / float(src_bytes)) ** (1.0 / 3.0)
        if byte_scale < scale:
            reason = f"max_bytes={max_bytes}"
        scale = min(scale, byte_scale)

    if scale >= 1.0:
        return src, False, ""

    dims = scaled_dims(src, scale)
    while dims.voxel_count > max_bytes:
        scale *= 0.995
        dims = scaled_dims(src, scale)

    return dims, True, reason


def source_z_for_output(out_z: int, out_depth: int, src_depth: int) -> int:
    if out_depth <= 1:
        return 0
    return int(round((out_z * (src_depth - 1)) / (out_depth - 1)))


def write_volume(
    slices: Sequence[Path],
    src_dims: VolumeDims,
    dst_dims: VolumeDims,
    low: float,
    high: float,
    output_path: Path,
) -> None:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open("wb") as out_file:
        for z in range(dst_dims.z):
            src_z = source_z_for_output(z, dst_dims.z, src_dims.z)
            arr = load_slice(slices[src_z])
            arr_u8 = normalize_to_u8(arr, low, high)
            if (dst_dims.x, dst_dims.y) != (src_dims.x, src_dims.y):
                image = Image.fromarray(arr_u8, mode="L")
                image = image.resize((dst_dims.x, dst_dims.y), Image.Resampling.BILINEAR)
                arr_u8 = np.asarray(image, dtype=np.uint8)
            out_file.write(arr_u8.tobytes(order="C"))

            if (z + 1) % 200 == 0 or z + 1 == dst_dims.z:
                print(f"[write] {output_path.name}: {z + 1}/{dst_dims.z}")


def write_manifest(
    output_path: Path,
    dataset_id: str,
    spacing_mm: tuple[float, float, float],
    src_dims: VolumeDims,
    preview_blob_name: str,
    preview_dims: VolumeDims,
    full_blob_name: str,
    full_dims: VolumeDims,
    normalization: str,
    full_downsampled_from: VolumeDims | None,
) -> None:
    lines = [
        "cory_volume_manifest_version=1",
        f"dataset_id={dataset_id}",
        "voxel_format=r8_unorm",
        "endianness=little",
        f"spacing_mm={spacing_mm[0]},{spacing_mm[1]},{spacing_mm[2]}",
        f"source_dimensions={src_dims.x},{src_dims.y},{src_dims.z}",
        f"preview_blob={preview_blob_name}",
        f"preview_dimensions={preview_dims.x},{preview_dims.y},{preview_dims.z}",
        f"preview_byte_size={preview_dims.voxel_count}",
        f"full_blob={full_blob_name}",
        f"full_dimensions={full_dims.x},{full_dims.y},{full_dims.z}",
        f"full_byte_size={full_dims.voxel_count}",
        f"normalization={normalization}",
    ]
    if full_downsampled_from is not None:
        lines.append(
            f"full_downsampled_from={full_downsampled_from.x},{full_downsampled_from.y},{full_downsampled_from.z}"
        )
    output_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def append_catalog_entry(catalog_path: Path, manifest_path: Path) -> None:
    catalog_path.parent.mkdir(parents=True, exist_ok=True)
    rel_manifest = manifest_path
    if catalog_path.parent.exists():
        try:
            rel_manifest = manifest_path.relative_to(catalog_path.parent)
        except ValueError:
            rel_manifest = manifest_path

    existing = []
    if catalog_path.exists():
        existing = [line.rstrip("\n") for line in catalog_path.read_text(encoding="utf-8").splitlines()]

    has_version = any(line.startswith("cory_volume_catalog_version=") for line in existing)
    dataset_line = f"dataset={rel_manifest.as_posix()}"
    if dataset_line in existing:
        return

    with catalog_path.open("a", encoding="utf-8") as file:
        if not existing:
            file.write("cory_volume_catalog_version=1\n")
        elif not has_version:
            file.write("cory_volume_catalog_version=1\n")
        file.write(dataset_line + "\n")


def main() -> None:
    args = parse_args()

    slices = discover_slices(args.input_dir, args.pattern, args.regex)
    print(f"Discovered {len(slices)} slices")

    (width, height), dtype = validate_stack(slices)
    src_dims = VolumeDims(width, height, len(slices))
    print(f"Source dimensions: {src_dims.x}x{src_dims.y}x{src_dims.z} dtype={dtype}")

    low, high, normalization = compute_normalization_window(
        slices, dtype, args.pct_low, args.pct_high, args.force_normalize
    )
    print(f"Normalization: {normalization} window=[{low}, {high}]")

    preview_dims = compute_preview_dims(src_dims, args.preview_max_dim)
    full_dims, full_downsampled, downsample_reason = compute_full_dims(
        src_dims, args.full_max_dim, args.full_max_bytes
    )
    print(f"Preview dimensions: {preview_dims.x}x{preview_dims.y}x{preview_dims.z}")
    print(f"Full dimensions: {full_dims.x}x{full_dims.y}x{full_dims.z}")
    if full_downsampled:
        print(
            f"WARNING: Full volume downsampled from {src_dims.x}x{src_dims.y}x{src_dims.z} "
            f"to {full_dims.x}x{full_dims.y}x{full_dims.z} due to {downsample_reason}"
        )

    args.output_dir.mkdir(parents=True, exist_ok=True)
    preview_blob = args.output_dir / f"{args.dataset_id}.preview.raw"
    full_blob = args.output_dir / f"{args.dataset_id}.full.raw"
    manifest_path = args.output_dir / f"{args.dataset_id}.cvol"

    write_volume(slices, src_dims, preview_dims, low, high, preview_blob)
    write_volume(slices, src_dims, full_dims, low, high, full_blob)

    write_manifest(
        manifest_path,
        args.dataset_id,
        tuple(args.spacing_mm),
        src_dims,
        preview_blob.name,
        preview_dims,
        full_blob.name,
        full_dims,
        normalization,
        src_dims if full_downsampled else None,
    )

    print(f"Wrote manifest: {manifest_path}")

    if args.append_catalog is not None:
        append_catalog_entry(args.append_catalog, manifest_path)
        print(f"Updated catalog: {args.append_catalog}")


if __name__ == "__main__":
    main()
