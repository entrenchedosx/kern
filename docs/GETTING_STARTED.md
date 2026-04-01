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
.\build\Release\kern.exe .\examples\hello.kn
```

REPL:

```powershell
.\build\Release\kern_repl.exe
```

Compiler tool:

```powershell
.\build\Release\kernc.exe --help
```

## IDE

Run the IDE from repo root:

```powershell
python .\kern-ide\main.py
```

## Portable “shareable” drops (Windows)

These are self-contained folders meant to run on machines without installing extra runtimes:

- `shareable-ide\compiler\` contains `kern.exe`, `kern_repl.exe`, `kernc.exe`, and `lib\`
- `shareable-kern-to-exe\` contains `kern.exe`, `kernc.exe`, and the `kern-to-exe` packager

Build them from repo root:

```powershell
powershell -ExecutionPolicy Bypass -File .\build_shareable_ide.ps1 -SkipNative
powershell -ExecutionPolicy Bypass -File .\scripts\publish_shareable_kern_to_exe.ps1
```

## Next docs

- [testing](TESTING.md)
- [troubleshooting](TROUBLESHOOTING.md)
- [release guide](..\RELEASE.md)

