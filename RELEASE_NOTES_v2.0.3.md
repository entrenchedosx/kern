# Kern v2.0.3 Release Notes

**Release Date:** April 25, 2026  
**Status:** Stable Production Release  
**Bytecode Schema:** 2.0.3

---

## 🎯 Major New Features

### 1. Vec3 Type - 3D Vector Support

Native 3D vector type for game development and graphics programming.

```kern
let pos = Vec3(1, 2, 3)
print(pos.x)  // 1
print(pos.y)  // 2
print(pos.z)  // 3
print(pos)    // Vec3(1, 2, 3)
```

**Features:**
- Constructor: `Vec3(x, y, z)` with numeric arguments
- Field access: `.x`, `.y`, `.z` properties
- Full runtime representation in VM
- Optimized opcodes: `BUILD_VEC3`, `VEC3_GET_X/Y/Z`

---

### 2. Minimal Structs - Type-Safe Data Structures

User-defined struct types with named fields and constructor functions.

```kern
struct Enemy {
    health: int
    speed: float
    name: string
}

// Positional arguments
let e1 = Enemy(100, 5.5, "Orc")

// Named arguments (using `=` or `:`)
let e2 = Enemy(name: "Goblin", health: 50, speed: 3.5)

// Field access
print(e2.name)    // Goblin
print(e2.health)  // 50
```

**Features:**
- Fixed fields (no inheritance, no generics)
- Automatic constructor generation
- Named argument support via existing infrastructure
- Runtime type metadata (`__kind`, `__name`)

---

### 3. Enhanced Named Arguments

Both `=` and `:` syntax now supported for named arguments.

```kern
// All of these work:
Entity(model = "sphere", position = (0, 0, 0))
Entity(model: "sphere", position: (0, 0, 0))
Entity(model: "sphere", position = (0, 0, 0))  // mixed
```

---

## 🔧 Implementation Details

### Compiler Changes
- **Parser:** Extended to accept `:` for named arguments (line 1282)
- **Codegen:** Struct constructor generation with proper function wrapping
- **Value System:** Added `Vec3Object`, `VEC3` type variant

### VM Changes
- **New Opcodes:**
  - `BUILD_VEC3` - Construct Vec3 from stack values
  - `VEC3_GET_X/Y/Z` - Field access opcodes
- **Bytecode Verifier:** Updated for Vec3 stack effects
- **Runtime:** Full `toString()`, `equals()`, `isTruthy()` support

### Build System
- Fixed legacy 3dengine references → migrated to g3d module
- Resolved CMake inconsistencies
- Stabilized cross-project dependencies

---

## 📦 Binaries

| Platform | Architecture | Filename | SHA256 |
|----------|-------------|----------|--------|
| Windows | x64 | `kern-v2.0.3-windows-x64.exe` | [See checksums below] |
| macOS | Apple Silicon | `kern-v2.0.3-macos-arm64` | Build from source |
| macOS | Intel | `kern-v2.0.3-macos-x64` | Build from source |
| Linux | x64 | `kern-v2.0.3-linux-x64` | Build from source |

**Note:** Windows binary is prebuilt. macOS and Linux binaries can be built from source using the provided CMake configuration.

---

## 🔒 SHA256 Checksums

```
# Windows x64
[TO_BE_COMPUTED_AFTER_BUILD]

# Source Archive
[TO_BE_COMPUTED_AFTER_BUILD]
```

---

## ⚡ Performance Notes

- Vec3 operations use dedicated VM opcodes (not function calls)
- Struct instances use existing map infrastructure (minimal overhead)
- Named argument reordering happens at compile time

---

## 📋 Backwards Compatibility

**Fully backwards compatible** - all existing Kern programs continue to work.

New features are additive only:
- `Vec3` is a new type, doesn't conflict with existing code
- Struct declarations use `struct` keyword (previously unused)
- Named argument `:` syntax is an addition to existing `=` syntax

---

## 🐛 Bug Fixes

- Fixed `kBytecodeFormatVersion` typo in `bytecode_header.hpp`
- Removed duplicate `kBytecodeSchemaVersion` definition
- Updated stale 3dengine module references to g3d
- Resolved struct constructor body execution during module init

---

## 🚀 Quick Start

```bash
# Windows
kern.exe myprogram.kn

# Test new features
kern.exe -e 'print(Vec3(1, 2, 3).x)'  # 1
```

---

## 📚 Documentation

- See `docs/kern_lean_roadmap.md` for future development plan
- See `examples/` for sample programs using new features

---

**Full Changelog:** Compare with v2.0.2 tag for detailed diff.
