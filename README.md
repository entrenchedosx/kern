# Kern

[![License: GPLv3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
[![Version](https://img.shields.io/badge/version-1.0.15-blue.svg)](KERN_VERSION.txt)
[![Discord](https://img.shields.io/badge/discord-Kern-5865F2?logo=discord&logoColor=white)](https://discord.gg/JBa4RfT2tE)

**Kern** is a practical **Python-like + C++-level system access** language: readable scripts, compiled to bytecode, executed by a fast VM, with explicit tooling for diagnostics, scanning, packaging, and deployment.

Use Kern when you want:
- **Python-style productivity** for day-to-day scripting and app logic.
- **C++-style system reach** (process, filesystem, networking, memory/interop).
- **Inspectability** (`--ast`, `--bytecode`, `--check`, scanner tooling) instead of black-box runtime behavior.
- **A real toolchain** (`kern`, `kernc`, `kern-scan`, tests, CI, docs) in one repo.

This repository is the **language and toolchain** (compiler, VM, stdlib, tests, docs). **Editors** (desktop IDE, VS Code extension) live under **[`Kern-IDE/`](Kern-IDE/)** and are published separately.

Kern is a **solo-built project by a 15-year-old developer**. That does not mean it is a toy: the repository includes a full compiler/VM pipeline, cross-platform CI, native module surfaces, and production-oriented tooling. Every major part of the language/toolchain here is authored and maintained independently.

**Discord:** [discord.gg/JBa4RfT2tE](https://discord.gg/JBa4RfT2tE) — official server for help and discussion.

---

## Table of contents

- [Community](#community)
- [Why Kern](#why-kern)
- [Quick start](#quick-start)
- [Features](#features)
- [Install](#install)
- [Usage](#usage)
- [Build from source](#build-from-source)
- [Project layout](#project-layout)
- [Repository architecture](#repository-architecture)
- [Documentation](#documentation)
- [Testing](#testing)
- [Releases & versioning](#releases--versioning)
- [Contributing](#contributing)
- [Attribution & trademark](#attribution--trademark)
- [Donations](#donations)
- [License](#license)

---

## Community

Official Kern Discord: **[discord.gg/JBa4RfT2tE](https://discord.gg/JBa4RfT2tE)** — help, language discussion, and tooling chatter.

---

## Why Kern

- **Readable surface:** familiar statements, functions, classes, and modules without a huge runtime.
- **Inspectable pipeline:** `--ast`, `--bytecode`, and `--check` for learning and debugging.
- **Solid diagnostics:** errors aim for file, line, hint, and stable codes (see [Troubleshooting](docs/TROUBLESHOOTING.md)).
- **System-first stdlib:** growing `std.v1.*` surface for fs/process/net/os/memory-style workflows.
- **Trust-based core:** Kern does not enforce a mandatory sandbox; power is explicit, and optional policy libraries exist for stricter designs — see [TRUST_MODEL.md](docs/TRUST_MODEL.md).
- **Optional graphics:** build with Raylib for `g2d` / `game` samples when you need them.

### Why click this repo?

- You get both the **language runtime** and the **full build/test/release toolchain**.
- It is active and iterative, with concrete roadmap execution and cross-platform CI.
- It targets real-world scripting + systems automation, not only toy examples.

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

- **Windows:** `.\install.ps1 -Mode User` (or `-Mode Global` in Administrator PowerShell).
- **Linux / macOS:** `chmod +x install.sh && ./install.sh --mode user` (or `--mode global` with `sudo`).

Installer capabilities:
- Detects existing `kern` in `PATH` and prints upgrade/downgrade/reinstall guidance.
- Supports `User`, `Global`, and `Portable` install modes.
- Installs versioned runtime layout (`versions/<version>/...`) plus active `bin/kern`.
- Performs SHA-256 integrity printout for installed binary.
- Updates PATH idempotently (no duplicate entries).

### Build from source

See **[docs/GETTING_STARTED.md](docs/GETTING_STARTED.md)** for CMake-only installs, vcpkg + static Raylib on Windows, and portable “shareable” drops.

---

## Usage

```text
kern [options] script.kn          # run (extension optional)
kern run script.kn               # explicit run
kern                             # REPL
kern --check [--json] [--strict-types] script.kn   # compile only; optional strict typing
kern --scan [.kn files or dirs]  # cross-layer scan (see docs/KERN_SCAN.md)
kern test [options] [directory]  # --grep, --list, --fail-fast; default tests/coverage
kern docs                        # documentation paths + optional MkDocs
kern build                       # CMake build hints (toolchain is CMake-built)
kern add pkg@^1.2.0              # add+resolve+install package (registry flow)
kern install [pkg@range]         # lockfile-aware install via API-first kern-registry
kern publish                     # publish current package to hosted registry API
kern search <query>              # package search (registry API/index)
kern info <pkg> [range]          # package metadata (registry API/index)
kern verify                      # kern.lock matches kern.json (CI)
kern --trace script.kn           # verbose VM trace (noisy)
kern --version / --help
```

### Package quickstart

```powershell
# set registry API (example local server)
$env:KERN_REGISTRY_API_URL = "http://127.0.0.1:4873"
# optional publish auth token (if server enforces auth)
# $env:KERN_REGISTRY_API_KEY = "your-token"

# in a Kern project folder
kern add example-http@^1.0.0
kern search http
kern info example-http

# publish this project as a package
kern publish
```

**Imports:** `import "math"`, `from "math" import sqrt`, or `import("path")` / `let m = import("math")`. Set **`KERN_LIB`** to the directory that **contains** `lib/kern` (often the repo root) so `import("lib/kern/...")` resolves when your working directory is not the project root.

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
| `kern/tools/` | C++ CLI entrypoints: `kern`, `kernc`, REPL, LSP, `kern-scan` (Phase 7) |
| `kern/modules/` | Native VM modules: `g2d/`, `g3d/`, `system/`, `game/` (+ builtin registry) |
| `kern/pipeline/backend/` | C++ backend / standalone emission (`kernc`, `kern-to-exe`) |
| `kern/core/utils/` | `kernconfig`, build cache |
| `src/` | Transitional glue: import/stdlib, process, system, scanner, packager, compile |
| `kern/core/compiler/` | Lexer, parser, semantic, codegen (Phase 3) |
| `kern/core/bytecode/` | Opcode/value model shared by compiler + VM |
| `kern/core/diagnostics/` | Shared diagnostic headers (Phase 4) |
| `kern/core/errors/` | Error reporter, VM error codes/registry contract, `errors.cpp` (Phase 5) |
| `kern/core/platform/` | `env_compat`, Windows `.kn` association (Phase 5) |
| `kern/pipeline/ir/` | Typed IR builders and optimization passes (Phase 6) |
| `kern/pipeline/analyzer/` | Project-wide / `kern.json` analysis (Phase 6) |
| `kern/runtime/vm/` | Interpreter, builtins implementation, verifier (Phase 2) |
| `lib/kern/` | Standard library (`.kn` and related assets) |
| `examples/` | Sample programs |
| `tests/` | Automated and regression tests |
| `docs/` | Guides, references, troubleshooting |
| `scripts/` | Maintainer automation (releases, checks, sync) |
| `tools/` | Small utilities, Windows launchers, site helpers (optional local `vcpkg` — see `.gitignore`) |
| `kargo/` | **GitHub-first** package CLI (`kargo install owner/repo@v…`); bundled by `install.ps1` / `install.sh` next to `kern` |
| `kern-registry/` | Optional **hosted** registry (Node API + `kern-pkg`); distinct from `kargo` |
| `kern-to-exe/` | Packager: `.kn` → standalone executable |
| `Kern-IDE/` | **Editor bundle** (Python/Qt/VS Code) — not required to build `kern` |
| `framework/` | Optional document-runtime demo (CMake-gated) |
| `3dengine/` | Optional 3D engine / ECS package (separate from core VM) when present |

The canonical version string is the root **`KERN_VERSION.txt`** file (used by `kern --version` and `kern_version()`).

---

## Repository architecture

High-level layout, dependency direction between compiler / VM / native modules, and a **phased plan** for migrating toward a `kern/{core,runtime,modules,...}` tree are documented in **[docs/architecture.md](docs/architecture.md)**. Root **`kern.toml`** holds a small path map for the same layout (metadata only; **CMake remains the build source of truth**).

---

## Documentation

| Doc | Contents |
|-----|----------|
| [architecture.md](docs/architecture.md) | Repository layers, current vs target paths, migration phases |
| [GETTING_STARTED.md](docs/GETTING_STARTED.md) | Build, run, portable drops |
| [TESTING.md](docs/TESTING.md) | Test scripts and stress suites |
| [TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md) | Common failures and fixes |
| [STDLIB_STD_V1.md](docs/STDLIB_STD_V1.md) | `std.v1.*` native modules |
| [KERN_SCAN.md](docs/KERN_SCAN.md) | `kern --scan` / `kern-scan` |
| [TRUST_MODEL.md](docs/TRUST_MODEL.md) | **Trust-the-programmer** stance and optional safety layers |
| [FEATURE_PLAN_20.md](docs/FEATURE_PLAN_20.md) | Backlog of 20 concrete features (phased) |
| [LANGUAGE_SYNTAX.md](docs/LANGUAGE_SYNTAX.md) | Short syntax overview + pointers to examples |
| [BUILTIN_REFERENCE.md](docs/BUILTIN_REFERENCE.md) | Where builtins are defined and how to look them up |
| [RELEASE_CHECKLIST.md](docs/RELEASE_CHECKLIST.md) | Maintainer pre-release checklist |
| [STANDALONE_COMPILE_ARCHITECTURE.md](docs/STANDALONE_COMPILE_ARCHITECTURE.md) | `.kn` → standalone `.exe` pipeline, `kern::compile` API |
| [RELEASE.md](RELEASE.md) | User install + maintainer packaging |
| [CHANGELOG.md](CHANGELOG.md) | Version history |
| [STRICT_TYPES.md](docs/STRICT_TYPES.md) | Optional `--strict-types` checks (Phase 2) |
| [LANGUAGE_ROADMAP.md](docs/LANGUAGE_ROADMAP.md) | Language evolution phases and status |
| [PRODUCTION_VISION.md](docs/PRODUCTION_VISION.md) | Phased plan for stability, diagnostics, stdlib, and tooling |
| [MEMORY_MODEL.md](docs/MEMORY_MODEL.md) | Value lifetimes and FFI boundaries |
| [ERROR_CODES.md](docs/ERROR_CODES.md) | Stable diagnostic codes |
| [IMPLEMENTATION_SUMMARY.md](docs/IMPLEMENTATION_SUMMARY.md) | Recent toolchain upgrade summary |
| [kern-registry/README.md](kern-registry/README.md) | Package registry architecture and command workflow |

---

## Testing

```powershell
.\build\Release\kern.exe test tests\coverage
```

See [docs/TESTING.md](docs/TESTING.md) for example sweeps, `kernc` tests, and stress runs. A full **go/no-go** script for releases is described in [RELEASE.md](RELEASE.md).

---

## Releases & versioning

- **Version:** `KERN_VERSION.txt` at repo root (e.g. `1.0.12`).
- **Changelog:** [CHANGELOG.md](CHANGELOG.md) follows [Keep a Changelog](https://keepachangelog.com/).
- **Tags:** release tags look like `v1.0.12` (see [docs/RELEASE_CHECKLIST.md](docs/RELEASE_CHECKLIST.md)).
- **CI:** [`.github/workflows/windows-kern.yml`](.github/workflows/windows-kern.yml), [`.github/workflows/linux-kern.yml`](.github/workflows/linux-kern.yml), and [`.github/workflows/macos-kern.yml`](.github/workflows/macos-kern.yml) build and smoke-test on pushes and PRs.
- **Releases:** Pushing a `v*` tag runs [`.github/workflows/release.yml`](.github/workflows/release.yml), which uploads **Windows** (zip, Raylib-enabled), **Linux** (`.tar.gz`, headless), and **macOS** (`.tar.gz`, headless) assets to the GitHub Release.

---

## Contributing

See **[CONTRIBUTING.md](CONTRIBUTING.md)** for coding expectations, how to run tests, and pull request basics.

---

## Attribution & trademark

- Attribution expectations: see [docs/ATTRIBUTION_POLICY.md](docs/ATTRIBUTION_POLICY.md).
- Naming/branding guidance: see [docs/TRADEMARK_POLICY.md](docs/TRADEMARK_POLICY.md).
- Contributor/project credits: see [CREDITS.md](CREDITS.md).
- Research/reference citation metadata: see [CITATION.cff](CITATION.cff).

---

## Donations

If Kern is useful to you, optional tips are welcome. Contributions directly support ongoing maintenance, release cadence, and new feature development; sustained funding helps keep long-term improvement work active. **Always verify** the address in your wallet before sending; on-chain transfers are irreversible.

| Network | Address |
|---------|---------|
| **Bitcoin** | `bc1qpzg22xmhzkchkwc96wxm9ge5x4a8kat7gxs6u7` |
| **Ethereum** | `0xE2F8Ac9D4e636115a47f0c12B42292a2A9E37f8F` |
| **Solana** | `9ivTpFL42Y7qJ8W66J1uycZyQXubHiyaWTv3o5Mj2gab` |

Donations are needed to accelerate development velocity and ship updates faster.

---

## License

[GNU GPL v3.0](LICENSE).
