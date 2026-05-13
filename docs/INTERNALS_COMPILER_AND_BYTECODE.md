# Internals — compiler and bytecode

## Phases (typical `kern script.kn`)

1. **Read source** — UTF-8; encoding helpers in [`kern/core/compiler/source_encoding.hpp`](../kern/core/compiler/source_encoding.hpp).
2. **Lex** — [`Lexer`](../kern/core/compiler/lexer.cpp) produces [`Token`](../kern/core/compiler/token.hpp) stream.
3. **Parse** — [`Parser`](../kern/core/compiler/parser.cpp) builds [`AST`](../kern/core/compiler/ast.hpp) (statements, expressions, imports).
4. **Semantic / project** — [`semantic.cpp`](../kern/core/compiler/semantic.cpp) and project-aware passes when enabled; see [LANGUAGE_ROADMAP.md](LANGUAGE_ROADMAP.md) for strict-type preview.
5. **Codegen** — [`CodeGenerator`](../kern/core/compiler/codegen.cpp) lowers AST to [`Opcode`](../kern/core/bytecode/bytecode.hpp) sequences and constant pools.
6. **Optimize (optional)** — [`bytecode_peephole`](../kern/core/bytecode/bytecode_peephole.cpp) applies safe, local rewrites after codegen.
7. **Verify (optional)** — [`bytecode_verifier`](../kern/runtime/vm/bytecode_verifier.cpp) checks invariants before execution.

**Static import graph (CLI):** `kern graph <entry.kn>` uses [`resolveProjectGraph`](../kern/core/compiler/project_resolver.cpp) (same resolver as `kernc` packaging) for a file-only dependency view; `--json` for tools.

**Bytecode schema id:** `kern::kBytecodeSchemaVersion` in [`bytecode.hpp`](../kern/core/bytecode/bytecode.hpp) is printed by **`kern --version`** as `bytecode-schema:`; bump it when changing serialized bytecode or opcode semantics incompatibly.

## Bytecode model

- **Instruction set** — `enum class Opcode` in [`kern/core/bytecode/bytecode.hpp`](../kern/core/bytecode/bytecode.hpp): stack ops, arithmetic, control flow (`JMP*`, `LOOP`), calls (`CALL`, `RETURN`, closures), objects (`GET_FIELD`, `NEW_OBJECT`), exceptions (`TRY_*`, `THROW`), `BUILTIN`, `UNSAFE_BEGIN/END`, `HALT`, etc.
- **Human names** — `opcodeName(Opcode)` must stay aligned with the enum (used by `kern --bytecode` and tooling).
- **Operands** — Encoded in the `Bytecode` container (see [`bytecode.hpp`](../kern/core/bytecode/bytecode.hpp) and VM dispatch in [`vm.cpp`](../kern/runtime/vm/vm.cpp)); indices reference **string** and **value** constant tables produced by codegen.

### Stability note

Treat **on-disk bytecode blobs** as tied to a **specific `kern` build**. Upgrades may change codegen or opcode details. For reproducible builds, pin **`KERN_VERSION.txt`** and archive the compiler binary with any stored bytecode.

## Builtins and indices

- Native calls use `Opcode::BUILTIN` with an **integer index** into the table wired in [`builtins.hpp`](../kern/runtime/vm/builtins.hpp).
- Policy: **append-only** indices for new builtins so existing serialized artifacts and scripts remain aligned with the registry expectations.

## Diagnostics

- Compile-time failures go through [`ErrorReporter`](../kern/core/errors/errors.hpp) with categories and stable codes (`LEX-*`, `PARSE-*`, `CODEGEN-*`, …).
- `kern --check --json` emits structured diagnostics for CI and IDEs.

## Optional IR / backend

- [`kern/pipeline/ir/`](../kern/pipeline/ir/) holds typed IR builders and passes (constant folding, DCE, etc.) used when feature flags enable those pipelines.
- [`kern/pipeline/backend/cpp_backend.cpp`](../kern/pipeline/backend/cpp_backend.cpp) is an alternate emission path for specific workflows—not the default `kern` execution path.
- [`src/compile/compile_pipeline.hpp`](../src/compile/compile_pipeline.hpp) — modular driver (`kern::compile`) for **standalone `.exe`** builds; design and trade-offs in [STANDALONE_COMPILE_ARCHITECTURE.md](STANDALONE_COMPILE_ARCHITECTURE.md).

## See also

- [STANDALONE_COMPILE_ARCHITECTURE.md](STANDALONE_COMPILE_ARCHITECTURE.md) — embedded VM backend, incremental cache, future bytecode/LLVM hooks
- [INTERNALS_VM.md](INTERNALS_VM.md) — how bytecode executes
- [ERROR_CODES.md](ERROR_CODES.md) — diagnostic codes
- [KERN_SCAN.md](KERN_SCAN.md) — static validation of exports and builtins
