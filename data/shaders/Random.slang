module Random;

// ============================================================
// PCG-inspired 3D/4D integer hash for GPU rendering
// Based on the "pcg3d/pcg4d" style hashes popularized in
// Jarzynski & Olano (JCGT) and commonly used in shaders.
// ============================================================

// 32-bit rotate left
public uint rotl32(uint x, uint r) { return (x << r) | (x >> (32u - r)); }

// Convert uint -> float in [0,1). Uses top 24 bits (mantissa-safe).
public float u01_from_u32(uint x)
{
    // Take the top 24 bits for a stable float mantissa distribution.
    // Result is in [0, 1). Never returns 1.0.
    return (float)(x >> 8) * (1.0 / 16777216.0); // 2^24
}

// PCG-style mix for 3D input producing 3D output (good for "spatial noise")
public uint3 pcg3d(uint3 v)
{
    // LCG step (per component) + cross-mixing
    v = v * 1664525u + 1013904223u;

    v.x += v.y * v.z;
    v.y += v.z * v.x;
    v.z += v.x * v.y;

    v ^= (v >> 16);

    v.x += v.y * v.z;
    v.y += v.z * v.x;
    v.z += v.x * v.y;

    return v;
}

// 4D variant: feed (x,y,z, w) where w can be frame/sample/dimension
public uint4 pcg4d(uint4 v)
{
    v = v * 1664525u + 1013904223u;

    v.x += v.y * v.w;
    v.y += v.z * v.x;
    v.z += v.x * v.y;
    v.w += v.y * v.z;

    v ^= (v >> 16);

    v.x += v.y * v.w;
    v.y += v.z * v.x;
    v.z += v.x * v.y;
    v.w += v.y * v.z;

    return v;
}

// ------------------------------------------------------------
// Stateless spatial noise: position + "key" -> float [0,1)
// ------------------------------------------------------------

// Recommended: quantize position to a stable integer lattice BEFORE hashing.
// For volumes: you often want voxel indices or brick-local indices.
// posI: integer coordinates (e.g. voxel coords)
// key : pack frame/sample/dimension into one uint (or just a counter)
// Returns: spatially well-distributed white noise
public float spatialNoise01(uint3 posI, uint key)
{
    uint4 h = pcg4d(uint4(posI, key));
    // Any component is fine; w tends to be well-mixed too.
    return u01_from_u32(h.w);
}

// Generate two independent uniforms (e.g. for delta tracking loop):
//  - u_dist  : free-flight
//  - u_accept: accept/reject
public float2 spatialNoise02(uint3 posI, uint key)
{
    uint4 h = pcg4d(uint4(posI, key));
    return float2(u01_from_u32(h.x), u01_from_u32(h.y));
}

// ------------------------------------------------------------
// Counter-based usage pattern (no per-thread RNG state required)
// ------------------------------------------------------------

// Example: build a per-path key and then just increment "dim" for each draw.
// pixelID  : dispatch thread id flattened
// frameIdx : current frame
// sampleIdx: per-pixel sample index (if you have it)
// bounce   : ray depth / stage id (optional)
public uint makePathKey(uint pixelID, uint frameIdx, uint sampleIdx, uint bounce)
{
    // Mix these however you like; the pcg4d will do the heavy lifting.
    // Keep it simple: a few xors/mults with odd constants.
    uint k = pixelID;
    k ^= frameIdx * 0x9E3779B9u;
    k ^= sampleIdx * 0x85EBCA6Bu;
    k ^= bounce   * 0xC2B2AE35u;
    return k;
}

// Per-random-draw dimension/counter. You can keep this in a register and ++ it.
// dim should be different for: distance vs accept vs phase vs light, etc.
public float rand01_spatial(uint3 posI, uint pathKey, uint dim)
{
    // Combine pathKey and dim so each "dimension" is distinct.
    uint key = pathKey ^ (dim * 0xA511E9B3u);
    return spatialNoise01(posI, key);
}