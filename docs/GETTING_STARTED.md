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

