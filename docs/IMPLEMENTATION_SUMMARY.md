# Kern upgrade — implementation summary

For **architecture and production-oriented internals** (not tied to a single changelog), see [INTERNALS.md](INTERNALS.md) and the linked **Internals** docs in the MkDocs nav.

## 1. Selective import syntax (Python-style)

**Introduced:**

```text
from "module" import sym1, sym2, ...
```

- Loads the module via `__import`.
- Binds requested symbols directly into the global scope.
- Stays consistent with `import "math"`, `let m = import("math")`, and `m["sqrt"]`.

**Implementation details:**

- `ImportStmt` extended with `namedImports`.
- New parser entry: `Parser::fromImportStatement()`.
- Codegen uses temporary global `__kern_selimp__`, cleared after binding.

## 2. `from` as a reserved keyword

- Added `from` → `TokenType::FROM` in the lexer.
- Parsing updated for correctness and clarity: `extern def ... from "dll"` now uses strict token consumption.
- Reduces ambiguity and improves grammar consistency.

## 3. Tooling and static analysis integration

Ecosystem compatibility:

| Area | Change |
|------|--------|
| **Dependency resolution** | `project_resolver.cpp` detects `from ... import ...` |
| **Static analysis** | `project_analyzer.cpp` registers each imported symbol individually |
| **Semantic layer** | `semantic.cpp` recognizes `FROM` in keyword contexts |

## 4. Memory model (documented and defined)

**Added:** `docs/MEMORY_MODEL.md`

**Core characteristics:**

- Reference-counted `Value` handles.
- No user-facing garbage collector.
- Controlled FFI / unsafe boundaries.
- Single-threaded VM semantics (including async helpers).

## 5. Developer experience improvements

**REPL**

- Commands: `trace on` / `trace off`, `.trace on` / `.trace off`.
- **`last` / `.last`**: prints the last diagnostic (code, message, optional stack frames).
- Updated help output and examples.

**VM debug controls**

- `VM::setVmTraceEnabled(bool)`
- `VM::isVmTraceEnabled()`
- **`kern --trace`**: enables VM instruction tracing for the following script run or REPL session (no env var required).

**CLI**

- `kern --help` updated with the new import syntax.
- **`kern verify`**: exit 0 when `kern.lock` dependency names match `kern.json` (CI-friendly).
- `kern doctor` reports key docs when present (`MEMORY_MODEL.md`, `ERROR_CODES.md`, `IMPLEMENTATION_SUMMARY.md`, roadmap).

**Diagnostics JSON**

- Runtime stack frames in JSON include a **`filename`** field for each frame (aligned with roadmap “diagnostics v2”).

## 6. Runtime clarification (concurrency model)

`__spawn_task` is explicitly documented as:

- Cooperative (same VM execution).
- Not OS-level threading.

This avoids incorrect assumptions about parallelism.

## 7. Test coverage

**Added:** `tests/from_import_smoke.kn`

Verifies selective import from stdlib `math` and expected numeric results.

## Key design decisions

- **Selective imports reuse existing infrastructure** — no second resolution system; runtime stays simple (`GET_INDEX` reuse).
- **Keyword reservation (`from`)** — improves grammar reliability and reduces parsing edge cases.
- **No `async` / `await` lexer keywords in this pass** — avoids breaking existing identifiers; preserves backward compatibility.
- **Incremental evolution** — stability and integration over feature volume.

## Modified areas

| Area | Files |
|------|--------|
| Language core | `token.hpp`, `lexer.cpp`, `parser.hpp`, `parser.cpp`, `ast.hpp`, `codegen.cpp`, `semantic.cpp` |
| Project system | `project_resolver.cpp`, `project_analyzer.cpp` |
| VM / runtime | `vm.hpp`, `builtins.hpp` |
| CLI / REPL | `kern/tools/main.cpp`, `kern/tools/repl_main.cpp` |
| Diagnostics | `errors.cpp` (JSON stack `filename`) |
| Docs / tests | `docs/MEMORY_MODEL.md`, `docs/ERROR_CODES.md`, `docs/LANGUAGE_ROADMAP.md` (status §8), `tests/from_import_smoke.kn`, `tests/coverage/kern_verify_fixture/` |

## Breaking change

**`from` is now a reserved keyword** — it cannot be used as an identifier.

**Mitigation:** rename variables previously named `from`.

## Example usage

```kn
from "math" import sqrt, cos

print(sqrt(9.0))
print(cos(0.0))
```

## Scope and forward direction

This update focuses on fully integrated, production-ready improvements rather than partially implemented systems.

**Not included in this pass:**

- Full static type system with inference
- Major standard library expansion
- Garbage collector redesign
- True multithreaded runtime

These should be implemented in phased iterations, following `docs/LANGUAGE_ROADMAP.md`.
