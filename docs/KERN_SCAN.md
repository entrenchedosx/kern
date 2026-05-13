# Kern cross-layer scan (`kern --scan` / `kern-scan`)

The scan tool performs **one coordinated pass** across:

1. **Builtin registry (C++ → VM)** — duplicate names in `getBuiltinNames()`, missing `registerBuiltin` slots, empty entries (`SCAN-REG-*`).
2. **Stdlib module exports** — each `std.v1.*` export target must name a registered builtin (or an extra global alias such as `readFile`) (`SCAN-STDLIB-EXPORT`).
3. **Kern source** — lex, parse, codegen, bytecode-level **undefined global** hints (`ANAL-LOAD-GLOBAL`, same heuristic as `kern run`), plus the semantic pass used by `kern --check` (`--strict-types`).

## Usage

```text
kern --scan [options] [paths...]
kern-scan [options] [paths...]
```

- **paths**: files and/or directories. Directories are scanned recursively for `.kn`. Default is the current directory when omitted.
- **`--json`**: machine-readable diagnostics only on stdout (same schema as `kern --check --json`).
- **`--strict-types`**: enable the semantic strict typing pass.
- **`--verbose`**: print each file as it is scanned (stderr).
- **`--registry-only`**: skip `.kn` files; only run registry + stdlib export checks.

### Optional test run

- **`--test`** — after a successful static scan, runs the same suite as `kern test` (default directory `tests/coverage`).  
- **`--test <dir>`** — use that directory instead.

The test runner is invoked via a **sibling** `kern` / `kern.exe` next to the executable (typical in `build/Release/`).

## Exit codes

- **0** — no errors (warnings may still be present).
- **1** — one or more errors (including registry failures).
- **2** — invalid CLI flags.

## Troubleshooting

**`error: File not found: --scan (also tried --scan.kn)`**

The executable you ran does not implement `--scan` (stale install, or a `kern` on `PATH` that is not the one you built). The CLI then treats `--scan` as a script path.

1. **Rebuild** from the repo root: `cmake --build build --config Release --target kern kern-scan`
2. Run the **built** binary explicitly, e.g. `.\build\Release\kern.exe --scan --registry-only` (Windows) or `./build/kern --scan`.
3. Confirm **`kern --help`** lists `--scan`; if not, you are not running the updated toolchain.

## Source layout (C++)

| Path | Role |
|------|------|
| `src/scanner/scan_driver.cpp` | CLI parsing, directory walk, per-file compile + bytecode + semantic pass |
| `src/scanner/builtin_registry_check.cpp` | `getBuiltinNames()` duplicates / empty names; VM slot coverage via `VM::builtinSlotFilled` |
| `src/scanner/stdlib_export_check.cpp` | `stdV1NamedExports()` targets vs registered builtins |
| `kern/runtime/vm/vm.hpp` | `builtinSlotFilled` for registration checks |

Future work can split `scan_driver` into smaller translation units (e.g. per-rule analyzers) without changing the CLI.

## Notes

- **Not a full dependent-type checker**: call-site contracts for builtins are validated at **runtime** in the VM; the scanner adds a **maintainable hook** for cross-layer consistency (registry + exports) and reuses the compiler’s static analysis.
- For deeper project refactors (imports, fixes), use **`kernc --analyze`** / **`kern --analyze`** passthrough.
