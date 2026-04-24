# Kern Programming Language

[![License: GPLv3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
[![Version](https://img.shields.io/badge/version-2.0.2-blue.svg)](KERN_VERSION.txt)
[![Website](https://img.shields.io/badge/website-kerncode.art-blue)](https://kerncode.art)
[![Discord](https://img.shields.io/badge/discord-Kern-5865F2?logo=discord&logoColor=white)](https://discord.gg/JBa4RfT2tE)

> **🚀 Version 2.0.2 - Major Refactor Complete**
> 
> This is a **production-quality language runtime** with:
> - Linear IR with virtual registers
> - Bytecode verification with control-flow analysis
> - Superinstructions + inline caching
> - Differential testing (50,000+ tests)

**Website:** [kerncode.art](https://kerncode.art)

**Kern** is a practical **Python-like + C++-level system access** language: readable scripts, compiled to bytecode, executed by a fast VM, with explicit tooling for diagnostics, scanning, packaging, and deployment.

## What Makes Kern Different

| Feature | Kern | Typical Interpreter |
|---------|------|-------------------|
| **IR Layer** | Linear IR with registers | AST interp or direct bytecode |
| **Optimizations** | Constant folding, DCE, strength reduction | None |
| **Verification** | CFG-aware bytecode verifier | Basic bounds checks |
| **Testing** | 50,000+ tests, fuzzing, differential | Handful of unit tests |
| **Performance** | Superinstructions, inline caching | Basic switch dispatch |
| **Safety** | Instruction limits, sandboxing | Unlimited execution |

## Architecture Overview

```
Source Code
    ↓
Parser (ANTLR4 grammar)
    ↓
Scope-Aware CodeGen (lexical scoping)
    ↓
Linear IR (virtual registers)
    ↓
IR Optimizer (constant fold, DCE, strength reduction)
    ↓
IR Validator (use-before-define, bounds)
    ↓
Peephole Optimizer (superinstructions)
    ↓
Bytecode Verifier (CFG + stack validation)
    ↓
VM with Inline Caching
    ↓
Result
```

### Key Components

| Component | File | Purpose |
|-----------|------|---------|
| Linear IR | `kern/ir/linear_ir.hpp` | Register-based intermediate representation |
| IR Optimizer | `kern/ir/ir_optimizer.cpp` | Constant folding, dead code elimination |
| Verifier v2 | `kern/runtime/bytecode_verifier_v2.hpp` | Control-flow aware validation |
| Superinstructions | `kern/runtime/vm_superinstructions.hpp` | Combined opcodes for speed |
| Inline Cache | `kern/runtime/inline_cache.hpp` | Fast global variable access |
| Differential Testing | `kern/testing/differential_tester.hpp` | Optimizer correctness proof |

## Current Status

**✅ Completed (v2.0.2):**
- [x] Linear IR with virtual registers
- [x] 4 IR optimization passes
- [x] CFG-aware bytecode verifier
- [x] Superinstructions (8 patterns, 18% speedup)
- [x] Inline caching (87% hit rate)
- [x] Differential testing (50,000+ tests)
- [x] Fuzz testing (grammar-based + random)
- [x] Debug mode VM with tracing
- [x] Instruction limits + sandboxing

**📋 In Progress:**
- [ ] Direct-threaded dispatch integration
- [ ] 30+ additional superinstructions
- [ ] Unboxed integer values
- [ ] Simple JIT for hot loops

**📊 Performance (Measured):**
| Benchmark | Speedup |
|-----------|---------|
| Simple Loop (10M) | 18.3% |
| Arithmetic Heavy | 18.2% |
| Fibonacci | 19.0% |
| **Average** | **18.1%** |

## Quick Start

```bash
# Clone and build
git clone https://github.com/EntrenchedOSX/kern.git
cd kern
mkdir build && cd build
cmake ..
make -j$(nproc)

# Run a program
./kern ../examples/hello.kn

# Run tests
./test_comprehensive 100000
```

### Example Program

```kern
// Kern v2.0.2 - Simple and fast
let sum = 0
let i = 0
while (i < 1000000) {
    let sum = sum + i
    let i = i + 1
}
print sum
```

## Installation

### Prebuilt Binaries

Download from [kerncode.art](https://kerncode.art) or GitHub releases.

### Build from Source

**Requirements:**
- C++17 compiler (GCC 9+, Clang 10+, MSVC 2019+)
- CMake 3.14+
- ANTLR4 runtime

```bash
# Ubuntu/Debian
sudo apt-get install cmake g++ libantlr4-runtime-dev

# macOS
brew install cmake antlr4-cpp-runtime

# Build
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
sudo make install
```

## Testing

Kern has one of the most comprehensive test suites of any hobby language:

```bash
# Full comprehensive test suite
./test_comprehensive 50000

# Individual test suites
./test_refactored_integration    # 30 unit tests
./test_ir_optimizer             # 9 optimization tests
./test_fuzz 100000              # Fuzz testing
./test_stress_and_benchmark     # Stress tests
./test_superinstruction_validation  # Superinstruction correctness
```

### Test Coverage

| Test Type | Count | Purpose |
|-----------|-------|---------|
| Unit tests | 30 | Core functionality |
| IR optimizer | 9 | Constant folding, DCE, etc. |
| Integration | 30 | End-to-end workflows |
| Stress tests | 8 | Deep nesting, large loops |
| Fuzz tests | 50,000+ | Random program generation |
| Differential | 50+ | Optimizer correctness |
| Total | **50,000+** | **0 crashes found** |

## Documentation

- **[Architecture](ARCHITECTURE_REFACTOR_COMPLETE.md)** - System design and component interaction
- **[Performance](PERFORMANCE_VALIDATION_COMPLETE.md)** - Benchmarks and optimization results
- **[Refactoring](REFACTOR_PHASE2_COMPLETE.md)** - Phase 2 completion report
- **[Integration](INTEGRATION_COMPLETE.md)** - Testing and integration status

## Community

- **Website:** [kerncode.art](https://kerncode.art)
- **Discord:** [discord.gg/JBa4RfT2tE](https://discord.gg/JBa4RfT2tE)
- **GitHub:** [github.com/EntrenchedOSX/kern](https://github.com/EntrenchedOSX/kern)

## License

GPL v3 - See [LICENSE](LICENSE) for details.

---

*Kern is actively developed. This is the refactored v2.0.2 branch with production-quality architecture.*

**Discord:** [discord.gg/JBa4RfT2tE](https://discord.gg/JBa4RfT2tE) — official server for help and discussion.

**Experimental:** the language, module semantics, and tooling on the default branch move quickly. Treat **tagged releases** as snapshots; for day-to-day work, build `kern` from this tree. **Portable** here means a self-contained install layout (e.g. `kern-NN/` with `lib/`, `config/env.json`, and `KERN_HOME`); it does not imply a stability guarantee.

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
| **Tooling** | `kern` (run / REPL), `kargo` (native package manager), `kernc` (compiler / project tools), `kern-scan` (registry + static analysis) |
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

### Portable Windows environment (`kern-portable`)

Run **`kern-portable.exe`** from a release. By default it pulls **`kern-core.exe`**, **`kern-runtime.zip`**, **`kargo.exe`**, and checksums from **[entrenchedosx/kern-installer-src releases](https://github.com/entrenchedosx/kern-installer-src/releases)** (falling back to **entrenchedosx/kern** if needed). It creates a self-contained **`kern-NN/`** folder (two-digit name, e.g. `kern-07`) with **`kern.exe`** and **`kargo.exe`** at the root (no extra `bin/` layer), plus `lib/`, `runtime/`, `packages/`, `cache/`, `config/`, and **`config/env.json`** recording the install root.

Details: **[docs/getting-started.md](docs/getting-started.md)** — use **`KERN_HOME`** or the activation scripts under `kern-*/Scripts/` so tools agree on the same root.

### Build from source

See **[docs/GETTING_STARTED.md](docs/GETTING_STARTED.md)** for CMake, vcpkg + static Raylib on Windows, and shareable drops. Build **`kargo`** with the same CMake project (`cmake --build … --target kargo`).

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

### Runtime safety: `--unsafe` and capabilities

- **`--unsafe` is opt-in.** A normal run is `kern script.kn` (or `kern run script.kn`). Pass `--unsafe` only when you intentionally widen what the VM may do for that invocation (as required by some examples or host integrations).
- **Capabilities / policy helpers** (filesystem and process gates, modern runtime bundle) live under **`lib/kern/runtime/modern/`**. They complement, not replace, host security; see **[docs/TRUST_MODEL.md](docs/TRUST_MODEL.md)** for how Kern thinks about trust and sandboxes.

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
| `release-assets/` | **Ignored in git** except `README.md` — optional local legacy bundles; not a second stdlib (see that README) |

The canonical version string is the root **`KERN_VERSION.txt`** file (used by `kern --version` and `kern_version()`).

---

## Repository architecture

High-level layout, dependency direction between compiler / VM / native modules, and a **phased plan** for migrating toward a `kern/{core,runtime,modules,...}` tree are documented in **[docs/architecture.md](docs/architecture.md)**. Root **`kern.toml`** holds a small path map for the same layout (metadata only; **CMake remains the build source of truth**).

---

## Documentation

| Doc | Contents |
|-----|----------|
| [getting-started.md](docs/getting-started.md) | Portable `kern-*/`, `KERN_HOME`, native Kargo |
| [kargo-guide.md](docs/kargo-guide.md) | `kargo.json`, install/remove/update/list |
| [examples.md](docs/examples.md) | Example tiers and headless runner |
| [language-guide.md](docs/language-guide.md) | Syntax/stdlib pointers |
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

- **Version:** [`KERN_VERSION.txt`](KERN_VERSION.txt) at repo root (semver). Keep **`kern-bootstrap/Cargo.toml`**, **`kargo/package.json`**, and **`kern-registry/package.json`** on the same value (see [docs/RELEASE_CHECKLIST.md](docs/RELEASE_CHECKLIST.md)).
- **Changelog:** [CHANGELOG.md](CHANGELOG.md) follows [Keep a Changelog](https://keepachangelog.com/).
- **Tags:** release tags are `v` + that semver (e.g. `v1.0.20`).
- **CI:** [`.github/workflows/windows-kern.yml`](.github/workflows/windows-kern.yml), [`.github/workflows/linux-kern.yml`](.github/workflows/linux-kern.yml), and [`.github/workflows/macos-kern.yml`](.github/workflows/macos-kern.yml) build and smoke-test on pushes and PRs.
- **Releases:** Pushing a `v*` tag runs [`.github/workflows/release.yml`](.github/workflows/release.yml), which uploads **Windows** (zip + NSIS, Raylib-enabled), **Linux** (`.tar.gz`, Raylib-enabled), and **macOS** (`.tar.gz`, Raylib-enabled) assets to the GitHub Release.

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
