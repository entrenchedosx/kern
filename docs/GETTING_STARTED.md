# Getting started

## Build

From repo root:

```powershell
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target kern kern_repl kernc
```

If you need graphics/game modules, build with `KERN_BUILD_GAME=ON`. For Windows portable drops (no installs), use the shareable build scripts (see below).

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

## Editors (Kern-IDE)

Editor sources live under **`Kern-IDE/`** (desktop Tk, Qt, VS Code extension). From a checkout of this monorepo:

```powershell
cd Kern-IDE\desktop-tk
python main.py
```

Install the **Kern** toolchain first and ensure `kern` is on `PATH`, or set `KERN_EXE`. See [Kern-IDE/docs/INTEGRATION.md](../Kern-IDE/docs/INTEGRATION.md).

## Portable “shareable” drops (Windows)

These are self-contained folders meant to run on machines without installing extra runtimes:

- `shareable-ide\compiler\` contains `kern.exe`, `kern_repl.exe`, `kernc.exe`, and `lib\`
- `shareable-kern-to-exe\` contains `kern.exe`, `kernc.exe`, and the `kern-to-exe` packager

Build the **kern-to-exe** drop from repo root:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\publish_shareable_kern_to_exe.ps1
```

For a packaged desktop IDE, build from **`Kern-IDE/desktop-tk/packaging/`** (PyInstaller); see [Kern-IDE/README.md](../Kern-IDE/README.md).

## Next docs

- [testing](TESTING.md)
- [troubleshooting](TROUBLESHOOTING.md)
- [release guide](..\RELEASE.md)

