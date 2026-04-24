# Kern v2.0.1 Release Notes

## Release Date
2026-04-17

## ⚠️ Platform Support Notice

| Feature | Windows | Linux | macOS |
|---------|---------|-------|-------|
| **3D Engine (`3dengine`)** | ✅ Full Support | ❌ Not Available | ❌ Not Available |
| **Graphics (`g2d`)** | ✅ Full Support | ❌ Not Available | ❌ Not Available |
| **Core Language** | ✅ Full Support | ✅ Full Support | 🔨 Build from Source |
| **Standard Library** | ✅ Full Support | ✅ Full Support | 🔨 Build from Source |

> **Note:** Linux and macOS builds are CLI-only. The `3dengine` and `g2d` modules are unavailable on these platforms due to graphics dependencies.

## New Features (Windows)

### 3D Engine Module (`3dengine`)
Full-featured 3D graphics module with both high-level and low-level APIs.

**Ursina-Style API (Immediate Mode):**
```kern
let e = import("3dengine")
let u = e["ursina"]

let app = u["App"]()  // Creates world + camera + lighting
let cube = u["Entity"](app, {"model": "cube", "color": [1.0, 0.0, 0.0], "position": [0.0, 0.0, 0.0]})
```

**Low-Level API:**
- `world_create()` / `world_destroy()`
- `world_set_camera()` / `world_set_lighting()`
- `mesh_make()` / `mesh_update()`
- `material_make()` / `material_update()`
- `entity_create()` / `entity_set_transform()`

### API Conventions
- Handedness: Right-handed
- World Up: +Y
- NDC Depth: [-1, 1]
- Matrix Layout: Row-major

## Downloads

| Platform | File | Status | Direct Link |
|----------|------|--------|-------------|
| **Windows x64** | `kern-v2.0.1-windows-x64.zip` | ✅ Ready | https://github.com/entrenchedosx/kern/releases/download/v2.0.1/kern-v2.0.1-windows-x64.zip |
| **Linux x64** | `kern-v2.0.1-linux-x64.tar.gz` | ✅ Ready | https://github.com/entrenchedosx/kern/releases/download/v2.0.1/kern-v2.0.1-linux-x64.tar.gz |
| **macOS Universal** | `kern-v2.0.1-macos-universal.tar.gz` | 🔨 Build Required | Build from source using `releases/build-macos.sh` |

## Build Instructions

**Windows (Full Graphics):**
```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target kern
```

**Linux (CLI-only, no graphics):**
```bash
rm -rf build-linux && mkdir build-linux && cd build-linux
cmake .. -DCMAKE_BUILD_TYPE=Release -DKERN_BUILD_GAME=OFF
make -j$(nproc) kern
```

**macOS (CLI-only, no graphics):**
```bash
./releases/build-macos.sh
```

## Verification

**Windows (with graphics):**
```kern
// Test 3dengine import
let e = import("3dengine")
print(e["VERSION"])  // "0.1.0"
```

**Linux/macOS (CLI-only):**
```kern
// Test core functionality
print("Hello from Kern!")
let arr = array(1, 2, 3)
print(len(arr))  // 3

// 3dengine will return null
let e = import("3dengine")
print(e)  // null (not available)
```
