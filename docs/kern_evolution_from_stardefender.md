# Kern: Evolution from Star Defender Pain Points

## Overview

This document traces the origin of Kern's feature set to concrete pain points encountered during the development of a 2D game called "Star Defender" in an earlier version of the language. Each pain point motivated a specific language feature, and all six features are now fully implemented.

---

## Issue 1: Named Arguments Not Supported

**Problem**: Function calls with multiple positional parameters were error-prone and unreadable. In a game context, calls like `spawn_enemy(100, 200, 3, true, 0.5, "scout")` required constant reference to function signatures.

**Solution**: Named arguments. Function calls can use `name: value` syntax, allowing:
```kern
spawn_enemy(x: 100, y: 200, wave: 3, is_boss: false, speed: 0.5, type: "scout")
```

**Implementation**: Parser-level feature. The parser (in [`kern/core/compiler/parser.cpp`](../kern/core/compiler/parser.cpp)) maps named arguments to positional parameters at parse time. Represented in the AST as [`CallArg`](../kern/core/compiler/ast.hpp:77) nodes with an optional `name` string.

**Status**: COMPLETED

---

## Issue 2: Dictionaries Instead of Structs

**Problem**: Game objects were represented as bare dictionaries (`{ "x": 100, "y": 200, "hp": 3 }`), requiring string-keyed access throughout the codebase. This approach was verbose, lacked type safety, and made refactoring hazardous -- renaming a field required finding every string literal that referenced it.

**Solution**: Structs with named fields and type annotations:
```kern
struct Entity {
    id: int,
    name: string,
    pos: vec3,
    hp: int,
    is_boss: bool
}

let e = Entity(id: 1, name: "boss", pos: vec3_new(0, 0, 0), hp: 100, is_boss: true);
```

**Implementation**: Struct declarations are parsed into [`StructDeclStmt`](../kern/core/compiler/ast.hpp:260) AST nodes. The code generator desugars struct construction into map construction and field access (`e.hp`) into map index operations. No runtime struct support is required in the VM.

**Status**: COMPLETED

---

## Issue 3: Too Many Global Position Variables

**Problem**: Position data was stored in separate global variables (`playerX`, `playerY`, `enemyX`, `enemyY`, `bulletX`, etc.), leading to dozens of global variables, name collisions, and no way to pass position data as a unit.

**Solution**: Unified `vec3` type with structured position data:
```kern
struct Entity {
    pos: vec3,
    // ...
}
let e = Entity(pos: vec3_new(100.0, 200.0, 0.0));
// Access: e.pos.x, e.pos.y, e.pos.z
```

**Implementation**: Native VM type stored in [`Value`](../kern/core/bytecode/value.hpp:45) as `Type::VEC3`. The [`Vec3Object`](../kern/core/bytecode/value.hpp:34) holds three doubles. Operations (add, sub, dot, normalize) are registered as built-in functions in [`kern/runtime/vm/vec3_builtins.hpp`](../kern/runtime/vm/vec3_builtins.hpp). Struct field layout metadata in the same file allows dot-access to vec3 components through struct instances.

**Status**: COMPLETED

---

## Issue 4: Manual Index-Based Loops

**Problem**: Iterating over arrays required manual index management:
```kern
let i = 0;
while i < len(enemies) {
    let enemy = enemies[i];
    // process enemy
    i = i + 1;
}
```

**Solution**: For-in loops:
```kern
for enemy in enemies {
    // process enemy
}
```

**Implementation**: The [`ForInStmt`](../kern/core/compiler/ast.hpp:407) AST node supports direct iteration over array values. The code generator emits loop control bytecode with automatic index management. Range-based (`for i in 0..10`) and C-style (`for i = 0; i < 10; i++`) loops are also supported.

**Status**: COMPLETED

---

## Issue 5: Manual Collision Math

**Problem**: Collision detection required verbose, error-prone manual vector math:
```kern
let dx = x2 - x1;
let dy = y2 - y1;
let dist = sqrt(dx * dx + dy * dy);
if dist < radius1 + radius2 {
    // collision
}
```

**Solution**: Vec3 built-in operations:
```kern
let diff = vec3_sub(pos2, pos1);
let dist = vec3_dot(diff, diff);  // squared distance
let dist = sqrt(dist);
if dist < collision_radius {
    // collision
}
```

**Implementation**: Five vec3 operations are available as built-in functions: `vec3_new`, `vec3_add`, `vec3_sub`, `vec3_dot`, `vec3_normalize`. These operate on the native [`Vec3Object`](../kern/core/bytecode/value.hpp:34) type and are dispatched directly through the VM's indexed builtin table.

**Status**: COMPLETED

---

## Issue 6: No Built-in Random Function

**Problem**: Generating random values required implementing custom pseudo-random number generators inline, adding boilerplate and inconsistency across the codebase.

**Solution**: Built-in `random()` function returning a pseudo-random floating-point value in the range [0.0, 1.0).

**Implementation**: Registered in the VM's builtin table at [`kern/runtime/vm/builtins.hpp`](../kern/runtime/vm/builtins.hpp).

**Status**: COMPLETED

---

## Side-by-Side Comparison

### Before (307 lines, fragile)

The original Star Defender implementation in early Kern relied on:
- Global variables for all positional data (~15 floats)
- Dictionary-based "entities" with string-keyed access
- Manual index tracking for all loops
- Inline collision math with raw dx/dy calculations
- Custom random number generation
- Positional-only function arguments requiring constant signature reference

### After (~150 lines, type-safe)

The improved implementation using modern Kern features:
- Structured entity types with named fields and type annotations
- Vec3 math for all position, velocity, and collision computations
- For-in loops eliminating index management
- Built-in random() function
- Named arguments for clear function calls
- Result types for error handling
- Defer for resource cleanup

---

## Implementation Timeline

All features were completed across 10 implementation rungs, with approximately 8 weeks of cumulative development:

| Phase | Feature | Duration |
|-------|---------|----------|
| Week 1 | Named arguments, random function | 2 features |
| Week 2-3 | For-in loops, Vec3 type | 2 features |
| Week 4-5 | Structs, module system | 2 features |
| Week 6 | Result type + `?` operator | 1 feature |
| Week 7 | Defer statements | 1 feature |
| Week 8 | Collections, integration testing | 2 features |

---

## Key Insight

Each of the six pain points from Star Defender was solvable with a focused, minimal language feature. By prioritizing features based on direct impact on game development ergonomics, the Kern language evolved into a practical tool for game scripting without accumulating speculative features. The total language surface remains intentionally small -- structs desugar to maps, UFCS desugars to function calls, and the `?` operator desugars to conditional branches -- demonstrating that expressive syntax need not require complex runtime support.
