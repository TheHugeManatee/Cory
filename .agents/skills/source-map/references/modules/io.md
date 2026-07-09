# IO

## Load When
- editing bitmap load/save helpers or image decoding used by tests and visual review
- adding BMP-related features (grayscale decode, BGRA read/write)

## Main Paths
- `src/Cory/IO/Bmp.hpp` — public API: structs + 5 functions (query/decode/load for grayscale; decode/loadBmpRgba8/writeBmpRgba8 for BGRA)
- `src/Cory/IO/Bmp.cpp` — BMP parser, palette identity fast-path, top-down support, BGRA↔RGBA conversion
- `tests/Bmp_Test.cpp` — Catch2 test covering info query, decode, load, write round-trip

## Important Concepts
- Three struct types: `BmpInfo`, `BmpImage` (grayscale R8), `BmpImageRgba8` (BGRA output)
- `queryBmpInfo` parses BMP bytes without loading a file; `decodeBmp` decodes 8-bit grayscale to R8 with palette identity fast-path and top-down support via negative height
- `loadBmp` combines query + decode into one BmpImage; `loadBmpRgba8` / `writeBmpRgba8` handle the BGRA path (BGRA layout, not ARGB or RGBA)
- Write creates parent directories if missing

## Read Next
- `src/Cory/IO/Bmp.hpp` — full public API surface
- `tests/Bmp_Test.cpp` — usage patterns and expected behavior

## Related Skills
- `testing` — when adding visual baselines that consume BMP I/O

## Gotchas
- Only 8-bit grayscale (with optional palette) and uncompressed 32-bit BGRA are supported — no color palettes, no 16/24 bpp
- BGRA layout is used for the 32-bit path; consumers must account for this ordering
