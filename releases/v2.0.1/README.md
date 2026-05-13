# Kern v2.0.1

## Release Assets

| Platform | File | Size |
|----------|------|------|
| Windows (Portable) | `kern-v2.0.1-windows-x64.zip` | ~4 MB |
| macOS | `kern-v2.0.1-macos-universal.tar.gz` | Build from source |
| Linux | `kern-v2.0.1-linux-x64.tar.gz` | Build from source |

## New in v2.0.1

### 3D Engine Module (`3dengine`)
- **Ursina-style API**: High-level immediate-mode 3D graphics
  - `App()` - Create window with auto world/camera/lighting
  - `Entity(app, props)` - Create 3D objects with one line
  - `Input` - Keyboard/mouse handling
- **Low-level API**: Direct world/camera/mesh/material control
- **Raylib backend**: Cross-platform rendering

### Usage

```kern
// Minimal 3D app
let e = import("3dengine")
let u = e["ursina"]

let app = u["App"]()
let cube = u["Entity"](app, {"model": "cube", "color": [1.0, 0.0, 0.0]})
```

## Build from Source

```bash
git clone https://github.com/entrenchedosx/kern
cd kern
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --target kern
```

## Checksums

```
SHA256(kern-v2.0.1-windows-x64.zip)=     [TO BE ADDED]
SHA256(kern-v2.0.1-macos-universal.tar.gz)= [TO BE ADDED]
SHA256(kern-v2.0.1-linux-x64.tar.gz)=     [TO BE ADDED]
```
