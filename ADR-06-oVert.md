# ADR-06: oVert Volume Data Import and Preview-First Streaming

## Status
Implemented (current as of 2026-02-15)

## Context
The volume rendering example (`examples/06-VolumeRendering`) originally rendered procedural data and assumed a uniform cube.
We needed a practical way to ingest oVert CT stacks from disk without adding per-format decoders in C++.

Goals:
1. Keep runtime ingestion simple and fast.
2. Support preview-first rendering while full data uploads asynchronously.
3. Preserve dataset aspect ratio from voxel spacing and dimensions.
4. Keep procedural generation as fallback when no dataset is configured.

## Decision
Adopt a two-stage import/runtime model:
1. Offline conversion script writes raw ingestible blobs plus a small JSON manifest.
2. Runtime loads manifests/catalog, streams preview first, then promotes to full volume.

The runtime format for this phase is fixed to `r8_unorm` (single-channel, tightly packed).

## Implemented Architecture

### File Formats
1. Dataset manifest: `.cvol` (JSON, UTF-8)
2. Dataset catalog: `.cvolcat` (line-based list of dataset manifests)

Implemented parsers:
1. `examples/06-VolumeRendering/src/VolumeManifest.hpp`
2. `examples/06-VolumeRendering/src/VolumeManifest.cpp`
3. `examples/06-VolumeRendering/src/VolumeCatalog.hpp`
4. `examples/06-VolumeRendering/src/VolumeCatalog.cpp`

### Converter (Offline)
Implemented script:
1. `tools/volume/convert_stack.py`

Outputs per dataset:
1. `<dataset_id>.preview.raw`
2. `<dataset_id>.full.raw`
3. `<dataset_id>.cvol`
4. Optional catalog append (`dataset=...`) to `.cvolcat`

Behavior:
1. Discovers image slices by numeric suffix and validates contiguous ordering.
2. Validates consistent source shape/dtype.
3. Converts to `r8_unorm` (identity for 8-bit by default, percentile normalization for high bit depth).
4. Always emits preview and full volumes.
5. Downsamples full volume only when constrained by `--full-max-dim` or `--full-max-bytes`.

### Runtime Streaming
Implemented service:
1. `examples/06-VolumeRendering/src/VolumeStreaming.hpp`
2. `examples/06-VolumeRendering/src/VolumeStreaming.cpp`

Flow:
1. Background thread reads preview/full blob bytes from disk.
2. Main/render thread creates 3D textures and enqueues transfers through `Cory::AsyncUploader`.
3. Preview becomes active first.
4. Full volume promotes when upload is complete.
5. Previous preview texture is retired safely after promotion.

### VolumeRendering Integration
Integration points:
1. `--volume-catalog` CLI option in `VolumeRenderDemo`.
2. `VolumeComponent.datasetId` selects streamed dataset.
3. Per-instance bindless texture index + dimensions in shader contract (`volumeMeta`).
4. Procedural fallback retained for entities with no dataset.

Important implementation detail:
1. Dataset entities now derive `VolumeComponent::size` from manifest physical extent:
   `physical_size = spacing_mm * source_dimensions` (with safe fallback to other manifest dimensions).
2. The physical extent is normalized so the longest axis maps to `4.0` scene units, preserving anisotropic proportions.

## Current Data Contract

### Required `.cvol` JSON Keys
1. `cory_volume_manifest_version` (integer, currently `1`)
2. `dataset_id` (string)
3. `voxel_format` (string, currently `r8_unorm`)
4. `endianness` (string, currently `little`)
5. `spacing_mm` (array of 3 numbers: `[sx, sy, sz]`)
6. `source_dimensions` (array of 3 unsigned integers: `[x, y, z]`)
7. `preview_blob` (string, relative path)
8. `preview_dimensions` (array of 3 unsigned integers)
9. `preview_byte_size` (unsigned integer, bytes)
10. `full_blob` (string, relative path)
11. `full_dimensions` (array of 3 unsigned integers)
12. `full_byte_size` (unsigned integer, bytes)
13. `normalization` (string, e.g. `identity`, `percentile:0.5,99.5`)

Optional:
1. `full_downsampled_from` (array of 3 unsigned integers)

Example:
```json
{
  "cory_volume_manifest_version": 1,
  "dataset_id": "g_meandricus_5",
  "voxel_format": "r8_unorm",
  "endianness": "little",
  "spacing_mm": [0.0298, 0.0298, 0.0298],
  "source_dimensions": [512, 512, 765],
  "preview_blob": "g_meandricus_5.preview.raw",
  "preview_dimensions": [128, 128, 192],
  "preview_byte_size": 3145728,
  "full_blob": "g_meandricus_5.full.raw",
  "full_dimensions": [384, 384, 512],
  "full_byte_size": 75497472,
  "normalization": "identity"
}
```

### Raw Blob Layout
1. Single channel (`uint8`)
2. X-major, then Y, then Z
3. Tight packing (no row/slice padding)

### `.cvolcat` Format
Required:
1. `cory_volume_catalog_version=1`
2. One or more `dataset=<relative/or/absolute/path/to/file.cvol>` entries

## Runbook (Generate + Run)

### 1. Session Setup
Run once per session from repo root:

```bash
./cbt start
```

### 2. Convert `G_maeandricus_5` Stack to `.cvol`
Install converter deps if needed:

```bash
python3 -m pip install --user Pillow numpy
```

Generate preview/full blobs and append to catalog:

```bash
python3 tools/volume/convert_stack.py \
  --input-dir "volumedata/Media 000451578 - Whole Body CTImageSeries CT/G_maeandricus_5" \
  --output-dir volumedata/converted \
  --dataset-id g_meandricus_5 \
  --pattern "*.bmp" \
  --spacing-mm 0.02983882 0.02983882 0.02983882 \
  --preview-max-dim 192 \
  --full-max-dim 512 \
  --full-max-bytes 300000000 \
  --append-catalog volumedata/converted/overt.cvolcat
```

### 3. Build VolumeRendering

```bash
./cbt build --target VolumeRendering
```

### 4. Run Headless Smoke (Recommended for CI/quick checks)

```bash
./cbt run VolumeRendering --headless --frames 220 --volume-catalog volumedata/converted/overt.cvolcat
```

Expected logs include:
1. `queued preview load`
2. `preview ready`
3. `uploading full volume`
4. `full volume ready`

### 5. Run Interactive

```bash
./cbt run VolumeRendering --frames 600 --volume-catalog volumedata/converted/overt.cvolcat
```

## Consequences
Positive:
1. No C++ source-format decoder complexity for medical stacks.
2. Fast initial visual feedback via preview-first strategy.
3. Async upload path avoids blocking the render loop.
4. Dataset anisotropy is now represented in scene-space volume size.

Tradeoffs:
1. Current runtime supports only `r8_unorm`.
2. Converter is required as a preprocessing step.
3. Very large datasets may still be downsampled based on configured caps.

## Non-Goals (Current Phase)
1. Runtime DICOM/PDF/other source decoders in C++
2. Compression/streaming bricks/virtual volume paging
3. Multi-channel scalar/vector volume formats

## References
1. `examples/06-VolumeRendering/src/VolumeRenderDemo.cpp`
2. `examples/06-VolumeRendering/src/VolumeRenderSystem.cpp`
3. `examples/06-VolumeRendering/src/VolumeStreaming.cpp`
4. `tools/volume/convert_stack.py`
5. `ADR-04-async-upload.md`
