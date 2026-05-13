# Getting Started with Kern

Welcome to Kern! This guide will walk you through everything you need to know to start writing powerful, elegant code.

## 🎯 What you'll get

- **`kern`** — The main interpreter: run `.kn` scripts, start REPL, compile checks, run tests
- **`kargo`** — Native package manager for managing dependencies and publishing packages
- **`kern-portable`** (Windows) — Self-contained installer that creates isolated environments
- **Complete toolchain** — Linter, scanner, debugger, and graphics support—all built-in

## 🚀 Quick Start (5 minutes)

**If you just want to try Kern right now:**

1. **Download** a prebuilt binary from [GitHub Releases](https://github.com/EntrenchedOSX/kern/releases)
2. **Run** a simple script:
   ```bash
   # On Windows
   kern.exe examples\basic\01_hello_world.kn
   
   # On Linux/macOS
   ./kern examples/basic/01_hello_world.kn
   ```
3. **Start the REPL** to experiment:
   ```bash
   kern
   ```

That's it! You're writing Kern code. 

For a proper installation, continue below.

## 📁 Understanding Kern's Environment

Kern is smart about finding its components. Here's how it locates everything:

### Resolution Order

1. **`kern --root <path>`** — Explicitly tell Kern where to find everything
2. **`KERN_HOME`** environment variable — Set this once and forget it
3. **Directory containing the executable** — For portable installations
4. **`config/env.json`** — Configuration file beside the executable
5. **Debug builds only** — Walk up directories looking for `lib/kern/`
6. **Cache file** — Remembered location for convenience

### What makes a "strict" root?

A valid Kern installation must have:
- `kern` and `kargo` binaries
- `lib/` directory with `lib/kern/` subdirectory
- `runtime/` directory

**For developers:** Use Debug builds to work from the source repository, or set `KERN_HOME` to point to a portable install.

## 🪟 Windows: Portable Installation

The portable installer is the easiest way to get started on Windows:

```powershell
# Download and install the latest version
.\kern-portable.exe init --latest
```

This creates a self-contained environment (e.g., `kern-42/`) with:
- `kern.exe` and `kargo.exe`
- Complete standard library
- Isolated package cache
- Configuration files

**Activate your environment:**

```powershell
# Use the folder name that the installer printed
. .\kern-42\Scripts\Activate.ps1

# Verify everything works
kern --version
kargo.exe --version
```

**Benefits of portable installs:**
- No admin rights required
- Multiple versions can coexist
- Completely isolated from system
- Easy to remove (just delete the folder)

## 🔨 Building from Source

### Prerequisites
- **C++17** compatible compiler
- **CMake 3.14+**
- **vcpkg** (recommended for dependencies)

### Windows Build

```powershell
# Configure with Visual Studio 2022
cmake -B build -G "Visual Studio 17 2022" -A x64 `
    -DCMAKE_TOOLCHAIN_FILE=...\vcpkg.cmake `
    -DVCPKG_TARGET_TRIPLET=x64-windows-static

# Build the core tools
cmake --build build --config Release --target kern kargo

# Test your build
.\build\Release\kern.exe examples\basic\01_hello_world.kn
```

### Linux/macOS Build

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --target kern kargo

# Test
./build/kern examples/basic/01_hello_world.kn
```

**🎮 Graphics support:** See [GETTING_STARTED.md](GETTING_STARTED.md) for Raylib setup instructions.

## 🎓 Next Steps

**Ready to write real Kern code?**

### 📖 Learn the Language
- **[Language Guide](language-guide.md)** — Complete syntax reference and feature overview
- **[Examples Tour](../examples/README.md)** — Hands-on learning with real code

### 📦 Package Management
- **[Kargo Guide](kargo-guide.md)** — Managing dependencies and publishing packages
- **[Package Registry](https://kerncode.art)** — Browse available packages

### 🎯 Try These First

```bash
# Start with the basics
kern examples/basic/01_hello_world.kn
kern examples/basic/05_functions.kn

# Explore modern features
kern examples/basic/18_modern_syntax.kn
kern examples/basic/22_pipeline_funtools.kn

# See advanced capabilities
kern examples/golden/golden_async_spawn_await.kn
kern examples/golden/golden_modern_runtime_commands_events.kn
```

### 🛠️ When You're Ready
- **Graphics**: `examples/graphics/` - 2D/3D programming with Raylib
- **System**: `examples/system/` - FFI, processes, and OS integration
- **Advanced**: `examples/advanced/` - Full applications and demos

**💡 Pro tip:** Start every new project from `examples/basic/` and build up gradually!
