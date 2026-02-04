# Slang Language Quick Primer

This document summarizes the **Slang** shading language with a focus on *what is different* compared to C/C++ and HLSL, how **idiomatic Slang code** is written, and which **pitfalls and mental model shifts** matter most. It is intended for readers already fluent in C-family languages and HLSL, but unfamiliar with Slang itself.

Slang is best understood as:
> **HLSL + modules + generics + interfaces + reflection‑friendly shader object model**

It is not just a syntax variant, but a language designed to scale shader authoring, specialization, and cross‑API portability.

## Contents

- [1. Core Philosophy](#1-core-philosophy)
- [2. Familiar Foundations (What Stays the Same)](#2-familiar-foundations-what-stays-the-same)
- [3. Modules and Imports (Major Difference)](#3-modules-and-imports-major-difference)
- [4. Types, Structs, and Defaults](#4-types-structs-and-defaults)
- [5. Functions, Overloading, and Methods](#5-functions-overloading-and-methods)
- [6. Generics (Templates Done Right)](#6-generics-templates-done-right)
- [7. Interfaces (Crucial Slang Feature)](#7-interfaces-crucial-slang-feature)
- [8. Resource and Parameter Model](#8-resource-and-parameter-model)
- [9. Shader Entry Points](#9-shader-entry-points)
- [10. Convenience Features Worth Noting](#10-convenience-features-worth-noting)
- [11. Idiomatic Slang Style Summary](#11-idiomatic-slang-style-summary)
- [12. Common Pitfalls](#12-common-pitfalls)
- [13. Mental Model Shift](#13-mental-model-shift)

---

## 1. Core Philosophy

Key design goals that shape the language:

- **Source‑level portability** across Vulkan, D3D12, Metal, CUDA‑like targets
- **Strong compile‑time abstraction** (generics, interfaces, specialization)
- **Explicit resource modeling** compatible with modern bindless APIs
- **Reflection‑driven runtime integration** (shader objects, parameter blocks)

Slang deliberately moves *policy* (binding, specialization, layout) out of handwritten boilerplate and into structured language features.

---

## 2. Familiar Foundations (What Stays the Same)

If you know HLSL, most surface syntax is familiar:

- C‑style expressions, control flow, structs, enums
- Vector/matrix types: `float3`, `float4x4`, etc.
- Entry points with stage attributes (`[shader("compute")]`, etc.)
- Resource types: `Texture2D`, `SamplerState`, `RWStructuredBuffer`, …
- Semantics (`SV_DispatchThreadID`, etc.)

If you know C++:

- Templates ≈ **generics**
- Interfaces resemble pure virtual base classes
- Namespaces and modules behave predictably

---

## 3. Modules and Imports (Major Difference)

Slang is **module‑based**, not file‑include‑based.

```slang
import math;
import render.common;
```

- `import` replaces most `#include` usage
- Modules compile independently and cache well
- Circular dependencies are disallowed

**Idiomatic Slang:**
- One logical module per file
- Minimal preprocessor usage
- Prefer `import` over `#include` unless macro behavior is required

---

## 4. Types, Structs, and Defaults

### Struct Member Defaults

Slang allows **default initializers** in structs:

```slang
struct Camera
{
    float4x4 viewProj = float4x4(1);
    float3 position;
    float exposure = 1.0;
}
```

This is *not* just syntactic sugar:
- Defaults propagate through parameter blocks
- Useful for specialization and partial overrides

### `let` vs `var`

```slang
let x = 4;   // immutable, compile‑time friendly
var y = x;   // mutable
```

- Prefer `let` for constants and derived values
- `let` enables better specialization and optimization

---

## 5. Functions, Overloading, and Methods

Functions resemble HLSL, but Slang adds:

### Member Functions

```slang
struct Ray
{
    float3 origin;
    float3 dir;

    float3 at(float t) { return origin + t * dir; }
}
```

### Operator Overloading

```slang
float3 operator +(float3 a, float3 b);
```

Used sparingly; idiomatic Slang prefers clarity over cleverness.

---

## 6. Generics (Templates Done Right)

Generics are first‑class and heavily used.

```slang
generic<typename T>
T lerp(T a, T b, float t)
{
    return a + (b - a) * t;
}
```

Key differences from C++ templates:

- No textual instantiation
- Fully type‑checked before specialization
- Works across shader stages and targets

**Idiomatic usage:**
- Generic math utilities
- Generic resource wrappers
- Algorithmic building blocks (scan, reduce, sort)

---

## 7. Interfaces (Crucial Slang Feature)

Interfaces are central to Slang’s abstraction model.

```slang
interface ILight
{
    float3 evaluate(float3 pos);
}
```

Types implement interfaces explicitly:

```slang
struct PointLight : ILight
{
    float3 position;
    float3 intensity;

    float3 evaluate(float3 p)
    {
        float d = length(p - position);
        return intensity / (d * d);
    }
}
```

### Why Interfaces Matter

- Enable **polymorphic shader code**
- Work with **existential types** (runtime‑selected implementations)
- Integrate with specialization and reflection

This replaces many macro‑based or `#ifdef` patterns common in HLSL.

---

## 8. Resource and Parameter Model

### Global Parameters

```slang
Texture2D<float4> gColor;
SamplerState gSampler;
```

### Constant Buffers

```slang
ConstantBuffer<Camera> gCamera;
```

### Parameter Blocks (Important)

```slang
ParameterBlock<MaterialParams> material;
```

Parameter blocks:
- Group parameters logically
- Map cleanly to descriptor sets / root signatures
- Are first‑class runtime objects

**Idiomatic Slang:**
- Prefer `ParameterBlock` over ad‑hoc globals
- Reflect layout automatically rather than hard‑coding bindings

---

## 9. Shader Entry Points

Entry points are ordinary functions annotated with attributes:

```slang
[shader("compute")]
[numthreads(8,8,1)]
void main(uint3 tid : SV_DispatchThreadID)
{
    // …
}
```

Multiple entry points per module are normal.

---

## 10. Convenience Features Worth Noting

### Type Inference

```slang
var n = normalize(v);
```

### Tuples

```slang
let (a, b) = foo();
```

### Optional Types

```slang
Optional<float> value;
```

### Pointers (Target‑Dependent)

```slang
float* ptr;
```

- Only valid on targets that support them (e.g., CUDA‑like backends)
- Avoid assuming pointer support in portable shaders

---

## 11. Idiomatic Slang Style Summary

Prefer:

- `import` over `#include`
- `let` over `var` where possible
- `ParameterBlock` over scattered globals
- Interfaces + generics over macros
- Struct defaults over manual initialization

Avoid:

- Heavy preprocessor logic
- API‑specific binding assumptions in shader code
- Duplicated shader variants via copy‑paste

---

## 12. Common Pitfalls

- **Assuming HLSL binding rules apply verbatim** (they don’t)
- **Overusing macros instead of interfaces/generics**
- **Forgetting that defaults affect layout and specialization**
- **Writing target‑specific code without capability checks**

Think in terms of *shader systems*, not single shaders.

---

## 13. Mental Model Shift

If HLSL is:
> “Write the shader the GPU will run”

Then Slang is:
> “Describe a shader *family* and let the compiler specialize it”

This shift is the key to writing effective, idiomatic Slang.
