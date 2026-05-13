# Kern + Kargo rebuild — delivery summary (Phase 11)

## 1. C++ audit

- Manifest: [`AUDIT_CPP_MANIFEST.md`](AUDIT_CPP_MANIFEST.md)
- Report: [`AUDIT_CPP_REPORT.md`](AUDIT_CPP_REPORT.md)

## 2. Examples audit

- [`AUDIT_EXAMPLES_REPORT.md`](AUDIT_EXAMPLES_REPORT.md)
- New: [`examples/basic/23_recursion_fib.kn`](../../examples/basic/23_recursion_fib.kn), [`examples/kargo/`](../../examples/kargo/)
- Runner: [`scripts/run_examples_headless.ps1`](../../scripts/run_examples_headless.ps1)

## 3. Documentation audit

- [`AUDIT_DOCS_REPORT.md`](AUDIT_DOCS_REPORT.md)
- New: [`docs/getting-started.md`](../getting-started.md), [`docs/language-guide.md`](../language-guide.md), [`docs/kargo-guide.md`](../kargo-guide.md), [`docs/examples.md`](../examples.md)
- Updated: [`README.md`](../../README.md), [`RELEASE.md`](../../RELEASE.md), [`docs/index.md`](../index.md), [`docs/GETTING_STARTED.md`](../GETTING_STARTED.md), [`docs/RELEASE_CHECKLIST.md`](../RELEASE_CHECKLIST.md)

## 4. Deleted / rewritten (high level)

- **Removed:** `.kern/bin` parent-walk delegation; Node-based portable Kargo unpack in `kern-portable-bootstrap` install path.
- **Rewritten:** `kern-portable-bootstrap` paths, install staging, doctor, `lib.rs` delegation (`KERN_HOME` for child).
- **New:** Native `kargo` CMake target; `kern_env` resolution (`KERN_HOME`, `--root`, cache file, project-local `config/package-paths.json`).

## 5. Final project structure (portable env)

```text
kern-<version>/
  kern.exe
  kargo.exe
  lib/
  runtime/
  packages/
  cache/
  config/
  Scripts/          # optional activation (KERN_HOME + PATH)
  config.toml
```

## 6. Key files

| Area | Files |
|------|--------|
| Env resolution | [`kern/core/platform/kern_env.hpp`](../../kern/core/platform/kern_env.hpp), [`kern_env.cpp`](../../kern/core/platform/kern_env.cpp) |
| Imports | [`src/import_resolution.cpp`](../../src/import_resolution.cpp) |
| CLI bootstrap | [`kern/tools/main.cpp`](../../kern/tools/main.cpp), `repl_main`, `scan_main`, `kernc_main` |
| Kargo | [`kern/tools/kargo_main.cpp`](../../kern/tools/kargo_main.cpp) |
| Portable | [`kern-portable-bootstrap/src/install.rs`](../../kern-portable-bootstrap/src/install.rs), [`paths.rs`](../../kern-portable-bootstrap/src/paths.rs), [`lib.rs`](../../kern-portable-bootstrap/src/lib.rs), [`doctor.rs`](../../kern-portable-bootstrap/src/doctor.rs) |
| Build | [`CMakeLists.txt`](../../CMakeLists.txt), [`.github/workflows/release.yml`](../../.github/workflows/release.yml) |

## 7. Build commands

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64 ...
cmake --build build --config Release --target kern kargo kern-portable   # last via Cargo: cargo build --release --manifest-path kern-portable-bootstrap/Cargo.toml
```

## 8. Architecture

Source → lexer → parser → semantic → codegen → bytecode → VM → builtins / native modules (`g2d`, `system`, …). Package imports: `config/package-paths.json` from native **kargo** or `KERN_HOME`.
