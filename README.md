# Kern

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Version](https://img.shields.io/badge/version-1.0.2-blue.svg)](VERSION)

**Kern** is a compiled scripting language aimed at **simplicity**, **readability**, and **control**: a small surface area, explicit flow, and a bytecode VM you can reason about. This repository contains the **language only** — compiler, VM, standard library, and CLI. Editors live in the separate **[Kern-IDE](Kern-IDE/)** tree (publish as its own GitHub repo).

## Features

- Custom **lexer / parser / semantic / codegen** pipeline and **bytecode VM**
- Standard library under **`lib/kern/`**
- **`kern`** CLI: run scripts, REPL, `--check`, project lockfile (`kern install`)
- Optional **graphics / game** modules when built with **`KERN_BUILD_GAME=ON`** (Raylib)
- **`kernc`** standalone compiler and tooling such as **`kern-to-exe/`**

## Project layout

| Path | Purpose |
|------|---------|
| `src/` | Compiler, VM, runtime, `main.cpp` entrypoints |
| `lib/kern/` | Standard library (`.kn` and related) |
| `examples/` | Sample programs and tests-by-example |
| `tests/` | Automated tests and regression suites |
| `docs/` | Guides (getting started, testing, troubleshooting) |
| `framework/` | Optional document-runtime demo (CMake gated) |
| `kern-to-exe/` | Packager: `.kn` → standalone executable |
| `Kern-IDE/` | **Editor bundle** (Tk desktop, Qt prototype, VS Code extension) — not part of the toolchain build |

The old nested `kern/` duplicate tree was removed; see [docs/NESTED_KERN_TREE_REMOVED.md](docs/NESTED_KERN_TREE_REMOVED.md).

## Installation

Build output is **`kern`** / **`kern.exe`** (and typically **`kernc`**, **`kern_repl`**, etc.) under your CMake build directory, e.g. `build/Release/kern.exe`.

### Windows (PowerShell)

```powershell
.\install.ps1 -AddToPath
kern --version
kern examples\basic\01_hello_world.kn
```

### Linux / macOS

```bash
chmod +x install.sh && ./install.sh
export PATH="$HOME/.local/bin:$PATH"
kern --version
kern examples/basic/01_hello_world.kn
```

See [docs/GETTING_STARTED.md](docs/GETTING_STARTED.md) for CMake-only installs and `make install`.

## Usage examples

```bash
kern --version
kern --help
kern script.kn              # run; resolves script.kn if extension omitted
kern run script.kn
kern --check script.kn      # compile-only / diagnostics
```

Shebang `#!/usr/bin/env kern` is skipped on the first line so `chmod +x script.kn` works on Unix.

## Build from source

**Requirements:** C++17, CMake 3.14+, optional vcpkg for Raylib.

```powershell
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target kern kernc
.\build\Release\kern.exe --version
```

With graphics (vcpkg + static Raylib on Windows — see [docs/GETTING_STARTED.md](docs/GETTING_STARTED.md)):

```powershell
cmake -B build -DKERN_BUILD_GAME=ON -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static
cmake --build build --config Release --target kern kernc
```

## Editors

Editor sources in this monorepo:

- `kern-ide/` — desktop Tk IDE (`main.py`, `app/`, `packaging/`)
- `editors/vscode-kern/` — VS Code extension

They invoke **`kern`** via subprocess/tasks; they do not embed this compiler.

## Windows portable / shareable folders

Maintainer-built drops may include:

- **`shareable-ide/compiler/`** — toolchain + `lib/` for `KERN_LIB`
- **`shareable-kern-to-exe/`** — packager + `kern` / `kernc`

IDE packaging is maintained under `kern-ide/packaging/`.

## Documentation

- [Getting started](docs/GETTING_STARTED.md)
- [Testing](docs/TESTING.md)
- [Troubleshooting](docs/TROUBLESHOOTING.md)
- [Release guide](RELEASE.md)

## License

MIT — see [LICENSE](LICENSE).
