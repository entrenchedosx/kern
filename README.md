# Kern

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Version](https://img.shields.io/badge/version-1.0.4-blue.svg)](VERSION)

**Kern** is a small, compiled scripting language: an explicit lexer → parser → bytecode pipeline and a bytecode **VM**, with a growing **standard library** (`lib/kern/`) and a **`kern`** CLI for running scripts, linting, and the REPL.

This repository is the **language and toolchain** (compiler, VM, stdlib, tests, docs). **Editors** (desktop IDE, VS Code extension) live under **[`Kern-IDE/`](Kern-IDE/)** and are published separately.

---

## Table of contents

- [Why Kern](#why-kern)
- [Quick start](#quick-start)
- [Features](#features)
- [Install](#install)
- [Usage](#usage)
- [Build from source](#build-from-source)
- [Project layout](#project-layout)
- [Documentation](#documentation)
- [Testing](#testing)
- [Releases & versioning](#releases--versioning)
- [Contributing](#contributing)
- [License](#license)

---

## Why Kern

- **Readable surface:** familiar statements, functions, classes, and modules without a huge runtime.
- **Inspectable pipeline:** `--ast`, `--bytecode`, and `--check` for learning and debugging.
- **Solid diagnostics:** errors aim for file, line, hint, and stable codes (see [Troubleshooting](docs/TROUBLESHOOTING.md)).
- **Optional graphics:** build with Raylib for `g2d` / `game` samples when you need them.

---

## Quick start

**Requirements:** a built `kern` / `kern.exe` on your `PATH`, or run from the CMake output directory.

```bash
kern --version
kern examples/basic/01_hello_world.kn
kern --check my_script.kn
```

On Windows from a fresh build:

```powershell
.\build\Release\kern.exe examples\basic\01_hello_world.kn
```

---

## Features

| Area | What you get |
|------|----------------|
| **Language** | Variables, `def`, classes, `match`, `try`/`catch`, modules via `import`, lambdas, and more |
| **Tooling** | `kern` (run / REPL), `kernc` (compiler / project tools), `kern-scan` (registry + static analysis) |
| **Stdlib** | Builtins + `lib/kern/*.kn`; versioned VM modules under `std.v1.*` — see [STDLIB_STD_V1.md](docs/STDLIB_STD_V1.md) |
| **Checks** | `kern --check`, `kern --scan`, `kern test` over `tests/coverage` |
| **Graphics** | Optional Raylib-backed `g2d` / `game` when built with `KERN_BUILD_GAME=ON` |

---

## Install

### Scripted install (recommended)

- **Windows:** `.\install.ps1 -AddToPath` (see script for options).
- **Linux / macOS:** `chmod +x install.sh && ./install.sh` then add `~/.local/bin` to `PATH` if prompted.

### Build from source

See **[docs/GETTING_STARTED.md](docs/GETTING_STARTED.md)** for CMake-only installs, vcpkg + static Raylib on Windows, and portable “shareable” drops.

---

## Usage

```text
kern [options] script.kn          # run (extension optional)
kern run script.kn               # explicit run
kern                             # REPL
kern --check [--json] script.kn  # compile only; JSON for IDEs
kern --scan [.kn files or dirs]  # cross-layer scan (see docs/KERN_SCAN.md)
kern test [directory]            # run .kn tests (default: tests/coverage)
kern --version / --help
```

**Imports:** set **`KERN_LIB`** to the directory that **contains** `lib/kern` (often the repo root) so `import("lib/kern/...")` resolves when your working directory is not the project root.

**Shebang:** a leading `#!/usr/bin/env kern` line is ignored on the first line so `chmod +x script.kn` works on Unix.

---

## Build from source

**Requirements:** C++17, CMake 3.14+. Optional: vcpkg / Raylib for graphics.

```powershell
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target kern kernc kern-scan
.\build\Release\kern.exe --version
```

Graphics-enabled build (vcpkg toolchain + static triplet on Windows — details in [GETTING_STARTED](docs/GETTING_STARTED.md)):

```powershell
cmake -B build -DKERN_BUILD_GAME=ON -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static
cmake --build build --config Release --target kern kernc kern-scan
```

---

## Project layout

| Path | Purpose |
|------|---------|
| `src/` | Compiler, VM, CLI entrypoints (`main.cpp`, …) |
| `lib/kern/` | Standard library (`.kn` and related assets) |
| `examples/` | Sample programs |
| `tests/` | Automated and regression tests |
| `docs/` | Guides, references, troubleshooting |
| `kern-to-exe/` | Packager: `.kn` → standalone executable |
| `Kern-IDE/` | **Editor bundle** (Python/Qt/VS Code) — not required to build `kern` |
| `framework/` | Optional document-runtime demo (CMake-gated) |

The canonical version string is the root **`VERSION`** file (used by `kern --version` and `kern_version()`).

---

## Documentation

| Doc | Contents |
|-----|----------|
| [GETTING_STARTED.md](docs/GETTING_STARTED.md) | Build, run, portable drops |
| [TESTING.md](docs/TESTING.md) | Test scripts and stress suites |
| [TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md) | Common failures and fixes |
| [STDLIB_STD_V1.md](docs/STDLIB_STD_V1.md) | `std.v1.*` native modules |
| [KERN_SCAN.md](docs/KERN_SCAN.md) | `kern --scan` / `kern-scan` |
| [LANGUAGE_SYNTAX.md](docs/LANGUAGE_SYNTAX.md) | Short syntax overview + pointers to examples |
| [BUILTIN_REFERENCE.md](docs/BUILTIN_REFERENCE.md) | Where builtins are defined and how to look them up |
| [RELEASE_CHECKLIST.md](docs/RELEASE_CHECKLIST.md) | Maintainer pre-release checklist |
| [RELEASE.md](RELEASE.md) | User install + maintainer packaging |
| [CHANGELOG.md](CHANGELOG.md) | Version history |

---

## Testing

```powershell
.\build\Release\kern.exe test tests\coverage
```

See [docs/TESTING.md](docs/TESTING.md) for example sweeps, `kernc` tests, and stress runs. A full **go/no-go** script for releases is described in [RELEASE.md](RELEASE.md).

---

## Releases & versioning

- **Version:** `VERSION` at repo root (e.g. `1.0.4`).
- **Changelog:** [CHANGELOG.md](CHANGELOG.md) follows [Keep a Changelog](https://keepachangelog.com/).
- **Tags:** release tags look like `v1.0.4` (see [docs/RELEASE_CHECKLIST.md](docs/RELEASE_CHECKLIST.md)).
- **CI:** [`.github/workflows/windows-kern.yml`](.github/workflows/windows-kern.yml) builds and smoke-tests on pushes and PRs; [`.github/workflows/release.yml`](.github/workflows/release.yml) attaches Windows binaries when a `v*` tag is pushed.

---

## Contributing

See **[CONTRIBUTING.md](CONTRIBUTING.md)** for coding expectations, how to run tests, and pull request basics.

---

## License

[MIT](LICENSE).
