# Changelog

All notable changes to Kern are documented here.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

---

## [Unreleased]

### Changed

- Documentation: simplified to a lean public set (`README.md`, `RELEASE.md`, `docs/GETTING_STARTED.md`, `docs/TESTING.md`, `docs/TROUBLESHOOTING.md`) and removed internal/extra markdown docs.
- `run_all_tests.ps1` now scans `examples/` **recursively** (matches `basic/`, `graphics/`, etc.).
- `docs/TESTING.md`: corrected path to `tests\kernc\run_kernc_tests.ps1`; documented `-Kernc` (alias `-Splc`).
- `docs/STRESS_TESTS_PLAN.md`: replaced with a pointer to `tests/coverage/` and `tests/run_all_coverage_kn.ps1`.
- `docs/TROUBLESHOOTING.md`: added VM reserved-opcode and example-runner notes.

### Fixed

- **Standalone EXE build (`kernc -o`) on Windows:** generated `CMakeLists.txt` now includes `http_get_winhttp.cpp` and links `winhttp`/`wininet`, matching the main `kern` target (fixes unresolved `kernHttpGetWinHttp` when building `kernc_standalone`).

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

---

## [Unreleased]

- Performance improvements and optional execution limits.
- Additional stdlib and module APIs as needed.
