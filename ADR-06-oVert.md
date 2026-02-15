# ADR-06 oVert Volume Data Import (Preview-First Streaming)

## Status
Proposed

## Context
`examples/06-VolumeRendering` currently raymarches a procedurally generated 3D texture.

We need to ingest oVert CT scan stacks from `volumedata/` without adding direct source-format
decoders in C++. The import path must:

1. Use a simple bespoke dataset format for runtime ingestion.
2. Load a small preview volume quickly.
3. Continue loading/uploading a larger volume in the background.
4. Promote to the large volume when ready without blocking rendering.

## Decision Summary
1. Introduce a text dataset manifest format (`.cvol`) that references raw single-channel blob files.
2. Introduce a catalog format (`.cvolcat`) to load multiple datasets at startup.
3. Runtime voxel format for this phase is fixed to `R8_UNORM`.
4. Each catalog dataset maps to one rendered `VolumeComponent` entity.
5. Loader behavior is preview-first, then full-resolution promotion.
6. Use `Cory::AsyncUploader::enqueueImageUpload` for GPU transfers.
7. Use a dedicated background thread for disk I/O.
8. Python conversion uses percentile normalization (`0.5%` / `99.5%` default) for high-bit-depth
   sources to `R8`.
9. Converter always outputs:
   - one small downsampled preview volume
   - one large volume that is directly ingestible by the app

## Public API / Interface Changes

### `examples/06-VolumeRendering` CLI
Add:

`--volume-catalog <path>`

If provided, catalog datasets are loaded and spawned as volume entities.
If omitted, existing procedural-volume fallback remains available.

### `VolumeComponent`
Extend `examples/06-VolumeRendering/src/Common.hpp` with dataset identity:

- `std::string datasetId`

### Shader/CPU Instance Contract
Update per-instance data in:

- `examples/06-VolumeRendering/src/VolumeRenderSystem.hpp`
- `examples/06-VolumeRendering/shaders/raymarch.comp.slang`

Add per-instance:

- bindless texture index
- active volume dimensions

This removes the single-global-volume assumption in the current raymarch path.

## File Format Specifications

### Dataset Manifest: `.cvol`
UTF-8 text, line-based `key=value`.

Rules:
1. `#` starts a comment.
2. Unknown keys are ignored with warning.
3. Paths are relative to the `.cvol` file directory.

Required keys:
1. `cory_volume_manifest_version=1`
2. `dataset_id=<string>`
3. `voxel_format=r8_unorm`
4. `endianness=little`
5. `spacing_mm=<sx>,<sy>,<sz>`
6. `source_dimensions=<x>,<y>,<z>`
7. `preview_blob=<relative path>`
8. `preview_dimensions=<x>,<y>,<z>`
9. `preview_byte_size=<bytes>`
10. `full_blob=<relative path>`
11. `full_dimensions=<x>,<y>,<z>`
12. `full_byte_size=<bytes>`
13. `normalization=percentile:0.5,99.5`

Optional keys:
1. `full_downsampled_from=<x>,<y>,<z>`

Raw blob memory layout:
1. Single channel, tightly packed.
2. X fastest, then Y, then Z.
3. Z order follows ascending numeric slice index from converter discovery.

### Dataset Catalog: `.cvolcat`
UTF-8 text, line-based `key=value`.

Required:
1. `cory_volume_catalog_version=1`
2. One or more `dataset=<relative/path/to/file.cvol>` entries

## Runtime Architecture

### New Types
Add example-local runtime helpers:

1. `VolumeManifest.hpp/.cpp`
2. `VolumeCatalog.hpp/.cpp`
3. `VolumeStreaming.hpp/.cpp`

### Dataset Streaming State Machine
Per dataset:

1. `PendingPreviewRead`
2. `PreviewUploading`
3. `PreviewReady`
4. `PendingFullRead`
5. `FullUploading`
6. `FullReady`
7. `Error`

### Threading Model
1. Background loader thread:
   - reads preview/full raw blobs from disk
   - validates byte count
   - posts `LoadedBlob` messages to main-thread queue
2. Main thread (`VolumeRenderSystem::beforeUpdate`):
   - consumes loaded blobs
   - creates GPU 3D textures/views
   - submits async upload tickets
   - polls `UploadTicket::ready()`
   - promotes active texture from preview to full when available

### Upload/Sync Contract
Use `AsyncUploader::ImageUploadRequest` with:

1. `oldLayout = Undefined`
2. `finalLayout = ShaderReadOnlyOptimal`
3. `finalStages = ComputeShaderBit`
4. `finalMask = ShaderReadBit`

`VolumeRenderSystem` must only bind dataset textures when their ticket is ready.

### Resource Lifetime During Promotion
When swapping preview -> full:

1. Keep old preview texture/view alive for at least `MAX_FRAMES_IN_FLIGHT`.
2. Retire safely after frame-lifetime delay (same pattern as deferred release helpers).

## Python Converter Specification

Add:

`tools/volume/convert_stack.py`

Dependencies:
1. `python3`
2. `Pillow`
3. `numpy`

### Inputs
1. Source stack directory
2. Slice selection pattern (glob or regex)
3. Output directory
4. `dataset_id`
5. Optional spacing (`sx,sy,sz` mm)
6. Preview size cap
7. Full-size caps
8. Percentile parameters

### Core Behavior
1. Discover slices, parse numeric index suffix, sort ascending.
2. Validate contiguous or fail with explicit error.
3. Validate all selected slices have same width/height and single-channel intent.
4. Convert to `R8`:
   - native 8-bit input: identity by default
   - high-bit-depth input: percentile window normalization (`0.5/99.5` default)
5. Always produce a small downsampled preview blob.
6. Produce a large blob intended for direct runtime ingestion:
   - keep source dimensions when within configured limits
   - downsample only when limits are exceeded
7. If downsampled, emit warning with source and output dimensions and limiting constraint.
8. Write:
   - `<dataset_id>.preview.raw`
   - `<dataset_id>.full.raw`
   - `<dataset_id>.cvol`
9. Optionally append `dataset=...` to a `.cvolcat`.

## Renderer Integration Plan
1. Parse catalog in `VolumeRenderDemo` when `--volume-catalog` is supplied.
2. Spawn one scene entity per dataset entry with `VolumeComponent.datasetId`.
3. Add `VolumeStreaming` ownership to `VolumeRenderSystem`.
4. Replace `volumeGenerationTask` path for loaded datasets with sampled uploaded textures.
5. Keep procedural generation path only as fallback when no catalog/dataset is active.
6. Update ImGui panel to show per-dataset load status:
   - loading preview
   - preview ready
   - loading full
   - full ready
   - error

## Testing Plan
1. Add parser tests in `tests/`:
   - valid `.cvol` and `.cvolcat`
   - missing required keys
   - malformed dimensions/byte sizes
   - unsupported version/format
2. Add converter tests (small synthetic stacks):
   - 8-bit passthrough
   - 16-bit percentile normalization
   - preview and full outputs have expected byte sizes
   - warning emitted when downsampling full
3. Add runtime smoke test:
   - run `VolumeRendering --headless --frames 20 --volume-catalog <path>`
   - verify no crash/assert and valid preview/full transition behavior
4. Keep existing no-catalog smoke path intact.

## Acceptance Criteria
1. `VolumeRendering` loads datasets from catalog and renders one entity per dataset.
2. Preview volume appears first while full volume is still processing.
3. Full volume replaces preview without blocking render loop.
4. Converted full blob is directly ingestible by app (no extra format conversion at runtime).
5. Parser/converter/runtime tests pass via `./cbt test` with focused filters.

## Assumptions and Defaults
1. Scope is limited to `examples/06-VolumeRendering` for now.
2. Runtime format is `R8_UNORM` only in this phase.
3. Blob paths are relative to manifest directory.
4. Compression and bricked virtual textures are out of scope.
5. `current-task.md` should be updated during implementation with final deviations/tradeoffs.

## References
1. `ADR-04-async-upload.md`
2. `examples/06-VolumeRendering/src/VolumeRenderSystem.cpp`
3. `src/Cory/Renderer/AsyncUploader.hpp`
