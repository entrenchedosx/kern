# Kern

Kern is a compiled scripting language with:

- a custom lexer/parser/codegen + VM runtime
- standard library modules under `lib/kern/`
- optional graphics/game support when built with game features (bundled into the Windows shareable builds)
- a lightweight Tk-based IDE in `kern-ide/`

## Core docs

- [getting started](docs/GETTING_STARTED.md)
- [testing](docs/TESTING.md)
- [troubleshooting](docs/TROUBLESHOOTING.md)
- [release guide](RELEASE.md)

## Quick start

```powershell
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target kern kern_repl kernc
.\build\Release\kern.exe --version
```

Run a script:

```powershell
.\build\Release\kern.exe .\examples\hello.kn
```

## Windows portable builds

This repo can produce self-contained Windows “shareable” folders (no extra runtime installs):

- `shareable-ide\compiler\` (kern toolchain + stdlib)
- `shareable-ide\ide.exe` (packaged IDE)
- `shareable-kern-to-exe\` (packager tool + compiler)

