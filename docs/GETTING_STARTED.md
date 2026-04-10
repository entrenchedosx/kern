# Getting started

> **Portable layout, `KERN_HOME`, and native Kargo:** see **[getting-started.md](getting-started.md)** for the current end-user flow. This page focuses on building from source (CMake, vcpkg, Raylib).

## Community

**Discord:** [discord.gg/JBa4RfT2tE](https://discord.gg/JBa4RfT2tE) — official Kern server for help, language discussion, and tooling.

## Build

From repo root:

```powershell
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target kern kern_repl kernc
```

Graphics/game modules (`import g2d`, `g3d`, `game`) are **included by default** (`KERN_BUILD_GAME=ON`). CMake **fails configuration** if Raylib cannot be resolved (vcpkg/static triplet on Windows, system packages + optional FetchContent on Linux/macOS). **Linux:** install X11/OpenGL/ALSA development packages first (same `apt install …` list as the `Install Raylib build dependencies` step in `.github/workflows/linux-kern.yml`). **macOS:** Xcode Command Line Tools are usually enough. **Windows:** use `.\build.ps1` or pass `-DCMAKE_TOOLCHAIN_FILE=…/vcpkg.cmake` and `-DVCPKG_TARGET_TRIPLET=x64-windows-static` (see `RELEASE.md`). For a headless toolchain only, configure with `-DKERN_BUILD_GAME=OFF`.

## Run

```powershell
.\build\Release\kern.exe .\examples\basic\01_hello_world.kn
```

REPL:

```powershell
.\build\Release\kern_repl.exe
```

Compiler tool:

```powershell
.\build\Release\kernc.exe --help
```

## Editors

Editor sources in this monorepo:

- `Kern-IDE/` — desktop Tk IDE (`main.py`, `app/`, `packaging/`)
- `editors/vscode-kern/` — VS Code extension

From repo root, run the Tk IDE:

```powershell
cd Kern-IDE
python main.py
```

Install the **Kern** toolchain first and ensure `kern` is on `PATH`, or set `KERN_EXE`. For imports like `import("lib/kern/...")`, set `KERN_LIB` to the folder that contains `lib/kern/`.

## Portable “shareable” drops (Windows)

These are self-contained folders meant to run on machines without installing extra runtimes:

- `shareable-ide\compiler\` contains `kern.exe`, `kern_repl.exe`, `kernc.exe`, and `lib\`
- `shareable-kern-to-exe\` contains `kern.exe`, `kernc.exe`, and the `kern-to-exe` packager

Build the **kern-to-exe** drop from repo root:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\publish_shareable_kern_to_exe.ps1
```

For a packaged desktop IDE, build from `kern-ide/packaging/` (PyInstaller).

## Next docs

- [testing](TESTING.md)
- [troubleshooting](TROUBLESHOOTING.md)
- [release guide](..\RELEASE.md)

