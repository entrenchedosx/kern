# Internals — compiler and bytecode

## Phases (typical `kern script.kn`)

1. **Read source** — UTF-8; encoding helpers in [`src/compiler/source_encoding.hpp`](../src/compiler/source_encoding.hpp).
2. **Lex** — [`Lexer`](../src/compiler/lexer.cpp) produces [`Token`](../src/compiler/token.hpp) stream.
3. **Parse** — [`Parser`](../src/compiler/parser.cpp) builds [`AST`](../src/compiler/ast.hpp) (statements, expressions, imports).
4. **Semantic / project** — [`semantic.cpp`](../src/compiler/semantic.cpp) and project-aware passes when enabled; see [LANGUAGE_ROADMAP.md](LANGUAGE_ROADMAP.md) for strict-type preview.
5. **Codegen** — [`CodeGenerator`](../src/compiler/codegen.cpp) lowers AST to [`Opcode`](../src/vm/bytecode.hpp) sequences and constant pools.
6. **Optimize (optional)** — [`bytecode_peephole`](../src/vm/bytecode_peephole.cpp) applies safe, local rewrites after codegen.
7. **Verify (optional)** — [`bytecode_verifier`](../src/vm/bytecode_verifier.cpp) checks invariants before execution.

**Static import graph (CLI):** `kern graph <entry.kn>` uses [`resolveProjectGraph`](../src/compiler/project_resolver.cpp) (same resolver as `kernc` packaging) for a file-only dependency view; `--json` for tools.

## Bytecode model

- **Instruction set** — `enum class Opcode` in [`src/vm/bytecode.hpp`](../src/vm/bytecode.hpp): stack ops, arithmetic, control flow (`JMP*`, `LOOP`), calls (`CALL`, `RETURN`, closures), objects (`GET_FIELD`, `NEW_OBJECT`), exceptions (`TRY_*`, `THROW`), `BUILTIN`, `UNSAFE_BEGIN/END`, `HALT`, etc.
- **Human names** — `opcodeName(Opcode)` must stay aligned with the enum (used by `kern --bytecode` and tooling).
- **Operands** — Encoded in the `Bytecode` container (see [`bytecode.hpp`](../src/vm/bytecode.hpp) and VM dispatch in [`vm.cpp`](../src/vm/vm.cpp)); indices reference **string** and **value** constant tables produced by codegen.

### Stability note

Treat **on-disk bytecode blobs** as tied to a **specific `kern` build**. Upgrades may change codegen or opcode details. For reproducible builds, pin **`KERN_VERSION.txt`** and archive the compiler binary with any stored bytecode.

## Builtins and indices

- Native calls use `Opcode::BUILTIN` with an **integer index** into the table wired in [`builtins.hpp`](../src/vm/builtins.hpp).
- Policy: **append-only** indices for new builtins so existing serialized artifacts and scripts remain aligned with the registry expectations.

## Diagnostics

- Compile-time failures go through [`ErrorReporter`](../src/errors.hpp) with categories and stable codes (`LEX-*`, `PARSE-*`, `CODEGEN-*`, …).
- `kern --check --json` emits structured diagnostics for CI and IDEs.

## Optional IR / backend

- [`src/ir/`](../src/ir/) holds typed IR builders and passes (constant folding, DCE, etc.) used when feature flags enable those pipelines.
- [`src/backend/cpp_backend.cpp`](../src/backend/cpp_backend.cpp) is an alternate emission path for specific workflows—not the default `kern` execution path.
- [`src/compile/compile_pipeline.hpp`](../src/compile/compile_pipeline.hpp) — modular driver (`kern::compile`) for **standalone `.exe`** builds; design and trade-offs in [STANDALONE_COMPILE_ARCHITECTURE.md](STANDALONE_COMPILE_ARCHITECTURE.md).

## See also

- [STANDALONE_COMPILE_ARCHITECTURE.md](STANDALONE_COMPILE_ARCHITECTURE.md) — embedded VM backend, incremental cache, future bytecode/LLVM hooks
- [INTERNALS_VM.md](INTERNALS_VM.md) — how bytecode executes
- [ERROR_CODES.md](ERROR_CODES.md) — diagnostic codes
- [KERN_SCAN.md](KERN_SCAN.md) — static validation of exports and builtins
