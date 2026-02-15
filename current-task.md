# Implement volume rendering

We implement direct volume rendering, with some extensions to make visual fidelity better.

# Plan

# Step 1:

Set up a basic volume raymarcher that can render a 3D texture using raymarching in a fragment shader.

- Create a 3D texture representing the volume data (e.g., a simple density field or a pre-defined dataset).
- Implement a compute shader that performs raymarching through the volume, accumulating color and opacity based on the
  sampled values.
- Set up a compute shader that performs the raymarching

# Step 2:

Extend the raymarcher to support basic lighting and transfer functions.

- Implement a simple lighting model (e.g., Phong or Blinn-Phong) to enhance the visual appearance of the volume.
- Introduce a transfer function that maps scalar values in the volume to color and opacity, allowing for better
  visualization of different structures within the volume.
- Add user controls to adjust the transfer function parameters in real-time.

# Step 3:

Temporal accumulation

- Add ray jittering to reduce banding artifacts
- Implement temporal accumulation to improve image quality over multiple frames

# Step 4:

Full Monte Carlo Volume Raycasting (optional, stretch goal)

- Implement Monte Carlo sampling techniques to improve the realism of the volume rendering.
- Explore advanced lighting models and scattering effects within the volume.
- Optimize performance to maintain interactive frame rates.

## Sidetracks

- Async parallel data loading
- Windows Release cppcoro coroutine ABI mismatch

# Current Status 
- Added initial oVert volume import pipeline (ADR-06):
  - New `.cvol` dataset manifest + `.cvolcat` catalog parsing in the VolumeRendering example.
  - Migrated `.cvol` manifest format from line-based key/value to JSON and switched loader parsing
    to `nlohmann::json` (simpler schema validation and lower parser complexity).
  - Replaced `VolumeStreaming` with `VolumeManagerSystem`:
    - Component-driven streamed volume ingestion via `StreamedVolume`.
    - Component-driven procedural generation via `ProceduralVolume`.
    - Background disk I/O thread reads preview/full raw blobs.
    - Main thread creates 3D textures and submits uploads through `AsyncUploader`.
    - Automatic preview -> full promotion with deferred preview resource retirement.
    - Manager writes runtime texture state into `VolumeComponent` (`textureView`,
      `textureDimensions`, `hasTexture`, `fullQuality`).
  - `VolumeRenderSystem` is now render-only:
    - No manifest registration or streaming ownership.
    - No compute-based fallback volume generation pass.
    - Raymarching consumes only runtime texture state from `VolumeComponent`.
    - Temporal accumulation history is copied to frame color before layer composition.
  - Wired `--volume-catalog` flow to spawn per-dataset entities with `StreamedVolume`
    and renderer settings on `VolumeComponent`.
  - Default scene now uses `ProceduralVolume` components for manager-driven generation.
  - Added `tools/volume/convert_stack.py` converter that outputs:
    - one small downsampled preview volume (`*.preview.raw`)
    - one larger app-ingestible volume (`*.full.raw`)
    - matching `.cvol` metadata manifest (with optional catalog append).
  - Validated against `G_maeandricus_5` dataset only (the larger sample remains intentionally
    excluded for now).
- Added a new core `Cory::IO::Bmp` loader module:
  - Self-contained decode path for uncompressed 24-bit/32-bit BMP files (top-down and bottom-up).
  - Public API for decode-from-memory and load-from-disk returning `Cory::Result<BmpImage>`.
  - Independent unit tests for happy paths and parse-failure cases.
  - Added checked-in test asset `tests/data/gray16x16_uncompressed_24bpp.bmp` (generated via Python)
    and a unit test that validates exact grayscale pixel values loaded in C++.
- Changedthe ResourceLocator::Locate interface to replace throwing exceptions with returning a Cory::Result
- Introduced async parallel dataset loading via `DatasetLoader` class
- Windows Release (`clion-release`) linker fix for cppcoro ABI mismatch:
  - Replaced the previous blanket override with a config-specific MSVC override on `cppcoro::cppcoro`:
    - `Release`: keep `/await` (package exports `std::experimental::coroutine_handle` ABI).
    - `Debug`: clear `/await` (package exports `std::coroutine_handle` ABI).
  - Verified both profiles by building `Cory_Tests`:
    - `./cbt build --profile clion-release --target Cory_Tests`
    - `./cbt build --profile clion-debug --target Cory_Tests`

## Open Investigation: Eager Task Declaration + Virtual Textures

- When `VolumeRenderSystem::volumeFrameTask` was refactored to eagerly declare all subtasks and branch only by output handle wiring, we hit a framegraph synchronization assertion in `synchronizeTexture` for `TEX_VolumeScratchDebugColor`.
- Symptom: barrier emission was attempted for a texture still in `TextureMemoryStatus::Virtual` (never allocated).
- Repro context: debug-raycast toggle / startup paths after switching to eager declaration.
- Likely cause: scratch textures were created in the parent task, while required work was performed by subtasks. If the parent task is not required by output resolution, those created resources are not pulled into required allocation, but required subtasks can still reference them.
- Decision for now: revert to conditional subtask declaration/wiring (pre-refactor behavior) to restore stability; investigate a general framegraph fix separately.

## Investigation: Direct Volume Writes (Host-Visible / BDA)

### Current state (baseline)
- `DatasetLoader` decodes all slices into one `std::vector<std::byte>` (`LoadedVolume::voxelsR8`).
- `VolumeManagerSystem` receives that CPU vector and uploads whole-volume data via `AsyncUploader` to a sampled `Texture3D`.
- `VolumeRenderSystem`/`raymarch.comp.slang` sample `gBindlessTexture3D[...]` by texture index.

### Key constraints discovered
- Buffer device address (BDA) applies to buffers, not sampled textures.
- Moving to true "direct write while loading" with BDA means the renderer must read volume from a buffer (manual sampling path), not from `Texture3D`.
- KDGpu exposes host-image-copy APIs (`Texture::hostLayoutTransition()` / `copyHostMemoryToTexture()`), but Cory currently does not request `hostImageCopy` in required device features.
- Mapping a sampled 3D texture directly is API-visible in KDGpu, but portability for sampled 3D + host-visible allocations is weaker than buffer-based mapping.

### Clean migration options
1. Minimal churn, texture remains authoritative:
   - Keep `Texture3D` sampling in shader.
   - Stream slices incrementally into the texture (one z-slice region at a time) instead of full-volume CPU staging.
   - Prefer host-image-copy when feature is enabled; otherwise use per-slice transfer uploads.
   - Removes full-volume intermediate CPU container in manager path.

2. Full BDA path (direct host-visible target, larger refactor):
   - Allocate one host-visible buffer for full volume (R8), decode each slice directly into mapped buffer offset.
   - Pass volume base address + dimensions to shader and implement manual 3D sampling (tri-linear in shader).
   - Removes sampled `Texture3D` dependency for streamed volumes.

### API cleanup targets (for full BDA path)
- Remove `LoadedVolume::voxelsR8` as a required transfer artifact for manager integration.
- Remove `VolumeManagerSystem::ReadResult::bytes` and `uploadBytesToVolume()` for streamed volume path.
- Remove streamed-volume dependence on `AsyncUploader` (keep uploader for other systems).
- Replace `VolumeComponent::textureView`-centric streamed contract with a buffer-address contract (while procedural path can stay texture-backed initially).

### Recommended sequencing
1. Introduce a sink-style `DatasetLoader` API (decode directly into caller-provided destination spans per slice).
2. Add streamed-volume buffer contract (`deviceAddress + dimensions`) alongside existing texture contract.
3. Switch raymarch shader to buffer sampling for streamed volumes.
4. Remove old full-volume CPU-vector transfer path and texture upload path for streamed datasets.

## Implemented: Slice-Reactive Partial Upload Path (Texture-backed)

- Added a slice-progress API in `DatasetLoader`:
  - `streamBmpStack(request, onSliceLoaded)` emits per-slice `SliceLoadUpdate` callbacks while decode is running.
  - Existing `loadBmpStack()` now builds its full host volume on top of this streamed API for compatibility.
- `VolumeManagerSystem` now reacts to per-slice updates:
  - Receives slice payloads via callback queue from loader worker coroutines.
  - Allocates the target 3D texture once dimensions are known (first slice update).
  - Enqueues one partial upload per slice using existing `AsyncUploader` staging path:
    - region upload writes depth=1 at `z = sliceIndex` into the final texture.
  - Tracks progress (`expectedSlices`, `uploadedSlices`) and marks `FullReady` when all slices are uploaded and load completion is signaled.
- Removed no-longer-needed full-volume upload path in manager:
  - no longer waits for one giant `bytes` payload in `ReadResult`.
  - no longer calls whole-volume `uploadBytesToVolume()` for streamed datasets.

### Notes
- This keeps the texture sampling render path intact (no BDA shader rewrite), while enabling true slice-reactive background uploads.
- Memory pressure can still approach full-volume scale if decode outruns per-slice upload; bounded upload queueing is a future optimization.
