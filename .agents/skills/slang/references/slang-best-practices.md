# Slang Shader Best Practices

## Quick Triage Flow

1. Resolve parser/type errors first.
2. Re-check resource and parameter block declarations.
3. Verify stage-specific semantics and attributes.
4. Re-compile before broader optimization changes.

## Authoring Guidelines

- Prefer one logical module per file and explicit `import` statements.
- Keep shared math/material logic in helpers; keep entry points small.
- Prefer `let` for derived values that should remain immutable.
- Use `ParameterBlock<T>` to model grouped material/frame data.
- Favor interfaces/generics when you need variant behavior without macro sprawl.

## Readability and Maintainability

- Use descriptive names for spaces and frames (`worldPos`, `viewDirWS`, `uv01`).
- Separate data fetch, shading logic, and output packing into distinct blocks/functions.
- Keep control flow shallow in hot loops; avoid deeply nested branches.
- Comment only non-obvious math or target-specific constraints.

## Performance Hygiene (Portable)

- Hoist invariant calculations out of loops.
- Limit divergent branches in wave-sensitive regions.
- Avoid redundant normalization and expensive transcendentals in tight paths.
- Prefer coherent memory access patterns for texture/buffer reads.
- Keep thread-group dimensions intentional for compute kernels.

## Common Rewrite Patterns

### Replace macro variants with interfaces

Before (macro variants):

```slang
#if USE_POINT
float3 evalLight(float3 p) { ... }
#else
float3 evalLight(float3 p) { ... }
#endif
```

After (interface-driven):

```slang
interface ILight { float3 eval(float3 p); }

float3 shade<let T : ILight>(T light, float3 p)
{
    return light.eval(p);
}
```

### Replace scattered globals with a parameter block

Before:

```slang
Texture2D<float4> gAlbedo;
SamplerState gSampler;
float4 gTint;
```

After:

```slang
struct MaterialParams
{
    Texture2D<float4> albedo;
    SamplerState sampler;
    float4 tint;
}

ParameterBlock<MaterialParams> gMaterial;
```

## Diagnostic Hints

- "unresolved import": check module name/path and build include configuration.
- "no matching overload": inspect inferred types (`let` values, vector widths, scalar casts).
- Stage semantic mismatches: verify input/output structs and entry attributes together.
- Resource/layout mismatch symptoms: compare shader declarations against host-side bindings.
