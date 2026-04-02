# Changelog

All notable changes to Kern are documented here.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

---

## [Unreleased]

---

## [1.0.4] - 2026-04-02

### Fixed

- **CI:** Windows **Release** and **Windows Kern** workflows no longer run `kern test tests/coverage` or g3d window smokes on GitHub-hosted runners (no OpenGL / GLFW window). Use a local machine for full graphics + coverage runs.
- **CI:** Release workflow publishes the Windows zip to **GitHub Releases** via `softprops/action-gh-release` (tag push or manual dispatch).

### Changed

- Version bump only (supersedes failed automated publish for `v1.0.3` on headless runners).

---

## [1.0.3] - 2026-04-02

### Added

- **VM / modules:** Versioned **`std.v1.*`** stdlib modules (`std.v1.math`, `std.v1.string`, `std.v1.bytes`, `std.v1.collections`, plus fs/process/time re-exports); append-only **`std_*`** native builtins ([`src/vm/std_builtins_v1.inl`](src/vm/std_builtins_v1.inl), [`src/stdlib_stdv1_exports.hpp`](src/stdlib_stdv1_exports.hpp)).
- **CLI / tooling:** **`kern --scan`** and **`kern-scan`** — builtin registry + stdlib export validation + compile-time static analysis ([`docs/KERN_SCAN.md`](docs/KERN_SCAN.md)).
- **Kern library:** Large **`lib/kern/stdlib/`** catalog (algorithms, collections, graph helpers, encoding, text, time, …).
- **IDE sources:** **`Kern-IDE/`** desktop IDE layout (Python); VS Code extension under **`editors/vscode-kern/`**.
- **Docs:** [README](README.md) refresh, [CONTRIBUTING](CONTRIBUTING.md), [LANGUAGE_SYNTAX](docs/LANGUAGE_SYNTAX.md), [BUILTIN_REFERENCE](docs/BUILTIN_REFERENCE.md), [RELEASE_CHECKLIST](docs/RELEASE_CHECKLIST.md), [STDLIB_STD_V1](docs/STDLIB_STD_V1.md).
- **CI:** [`.github/workflows/release.yml`](.github/workflows/release.yml) — Windows build + zip artifact on **`v*`** tags; [windows-kern.yml](.github/workflows/windows-kern.yml) builds **`kern-scan`**, runs **`kern --scan --registry-only`** and smoke tests.

### Changed

- **Layout:** Editor and toolchain boundaries documented; see [NESTED_KERN_TREE_REMOVED.md](docs/NESTED_KERN_TREE_REMOVED.md) for history.

---

## [1.0.2] - 2026-04-02

### Added

- **Compiler / VM:** lambda **closure captures** (`BUILD_CLOSURE`, `FunctionObject::captures`); `CALL` appends captures after parameters (fixes callbacks like `g3.run` with lambdas over outer locals).
- **CLI:** `kern run <script>`; extensionless script path tries `<name>.kn`; UTF-8 BOM + shebang line strip for file runs; non-`.kn` scripts warned but allowed; `kern install` help text clarifies project lockfile vs system install.
- **Install:** `install.sh`, `install.ps1`, root `Makefile` `install` target, CMake `install(TARGETS kern)`; Windows `.reg` and Linux MIME helpers under `tools/`.
- **`kern::process` (Windows):** `first_readable_region` (VirtualQueryEx); `system_process_safe_read.kn` example; module field `_kern_process_api` for introspection.

### Fixed

- **Examples / tooling:** `system_process_safe_read` documents VA 0 reads and avoids non-ASCII punctuation in messages (Windows console mojibake).

---

## [1.0.1] - 2026-04-01

### Changed

- Documentation: simplified to a lean public set (`README.md`, `RELEASE.md`, `docs/GETTING_STARTED.md`, `docs/TESTING.md`, `docs/TROUBLESHOOTING.md`) and removed internal/extra markdown docs.
- `run_all_tests.ps1` now scans `examples/` **recursively** (matches `basic/`, `graphics/`, etc.).
- `docs/TESTING.md`: corrected path to `tests\kernc\run_kernc_tests.ps1`; documented `-Kernc` (alias `-Splc`); documented stress suite including **`-Aggressive`**.
- `docs/STRESS_TESTS_PLAN.md`: replaced with a pointer to `tests/coverage/` and `tests/run_all_coverage_kn.ps1`.
- `docs/TROUBLESHOOTING.md`: added VM reserved-opcode and example-runner notes.

### Fixed

- **Standalone EXE build (`kernc -o`) on Windows:** generated `CMakeLists.txt` now includes `http_get_winhttp.cpp` and links `winhttp`/`wininet`, matching the main `kern` target (fixes unresolved `kernHttpGetWinHttp` when building `kernc_standalone`).
- **Diagnostics:** runtime tracebacks cap printed frames and redundant snippets for deep stacks; **`stack_trace` / `stack_trace_array`** and **`attachTracebackToError`** use bounded snapshots (**256** innermost frames). CLI documents that **`kern`** overrides default max call depth (see `main.cpp`).

### Added

- **Stress suite (`tests/stress/`):** adversarial `.kn` samples, `run_stress_suite.ps1` (uses `Start-Process` for reliable exit codes on Windows), UTF-8 BOM / UTF-16 BOM lexer checks, generated long `??` / unary inputs, oversized source rejection, and a **VM max call-depth** run that must exit non-zero; **`-Aggressive`** scales generators, verifies depth failure under **`--release`**, and runs the other stress scripts end-to-end.
- **VM:** when `maxCallDepth_ > 0` (default 1024), **tail-call frame reuse is disabled** so recursion depth limits cannot be bypassed; `maxCallDepth_ == 0` means unlimited (and restores tail-call reuse). **`getCallStackSlice()`** for bounded reporting. **Lexer:** UTF-8 BOM strip + explicit **UTF-16 BOM** rejection via `source_encoding.hpp`.
- **Diagnostics:** lexer/parser/file-open detail lines use ASCII `-` bullets so Windows consoles do not show mojibake (e.g. `ΓÇó`); shared limits in `diagnostics/traceback_limits.hpp`.

### Security / robustness

- **Lexer:** reject sources over **48 MiB** and token streams over **8M tokens** with a clear `LexerError`; skip a leading **UTF-8 BOM** so Windows-saved sources tokenize cleanly.
- **Parser:** null-coalescing (`??`) parsed **left-associatively** without recursion (matches common semantics and avoids stack overflow on long chains); repeated unary `!`/`-`/`*` folded iteratively.

---

## [1.0.0] - 2025-03-07

### Added

- **Language**
  - Multi-paradigm: imperative, functional, OOP, pattern matching, destructuring.
  - Variables: `let`, `var`, `const`; optional type hints.
  - Control flow: `if`/`elif`/`else`, `for` (range, for-in, C-style), `while`, `match`/`case`, `try`/`catch`/`finally`, `throw`, `defer`, `assert`.
  - Functions: `def`, default and named args, multiple returns, lambdas, recursion.
  - Collections: arrays (literal, `array()`, slice, push), maps (literal, keys/values/has).
  - Ergonomics: optional chaining `?.`, null coalescing `??`, f-strings, ranges `1..10 step 2`.
  - Classes: `class`, `init`, `this`, `Instance()`, inheritance with `extends`.
- **Standard library**
  - Hundreds of builtins: math, strings, collections (map/filter/reduce, sort, zip, chunk, …), file I/O, JSON, random, time, env, reflection, profiling.
  - Modules: `import "math"`, `"string"`, `"json"`, `"sys"`, `"io"`, `"array"`, `"g2d"`, `"game"`, etc.
  - System: `repr`, `kern_version`, `platform`, `os_name`, `arch`, `cli_args`, `env_get`.
- **Graphics (optional, requires Raylib)**
  - g2d module: window, clear, setColor, drawRect, fillRect, drawCircle, fillCircle, drawLine, drawPolygon, drawText, run(update, draw).
  - game module: window, draw, input, sound, game loop.
- **CLI**
  - `kern script.kn` — run script.
  - `kern` — REPL.
  - `kern --version`, `kern -v` — print version.
  - `kern --help`, `kern -h` — print usage.
  - `kern --check <file>` — compile only (exit 0 if OK).
  - `kern --fmt <file>` — format source.
  - `kern --ast <file>`, `kern --bytecode <file>` — dump AST/bytecode.
- **Errors**
  - Line/column diagnostics, source snippets, hints, stack traces, categories (SyntaxError, RuntimeError, TypeError, etc.).
- **IDE**
  - Kern IDE (Electron + Monaco): edit, run, syntax highlighting; build with `npm run build:win` in kern-ide/.
- **Docs**
  - README, GETTING_STARTED, LANGUAGE_OVERVIEW, standard_library, TROUBLESHOOTING, kern_for_dummies.txt, RELEASE.md.

### Build

- CMake 3.14+; C++17. Optional Raylib for g2d/game (vcpkg or RAYLIB_ROOT).
- Version from root `VERSION` file; reflected in `--version` and `kern_version()`.
