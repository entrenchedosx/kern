# Internals — repository architecture

## High-level pipeline

```text
Source
  → Lexer (tokens)
  → Parser (AST)
  → Semantic / project (optional; flags)
  → IR / typed passes (optional; feature flags)
  → Codegen (bytecode + constant pools)
  → Verifier / peephole (optional)
  → VM (dispatch + builtins)
```

Not every run enables all **optional** stages; strict typing and IR passes depend on flags and `config/feature_flags.json`. The **default** CLI path is: tokenize → parse → (semantic hooks where enabled) → codegen → optional peephole → VM.

## Top-level `src/` map

| Area | Path | Role |
|------|------|------|
| **Driver / CLI** | [`kern/tools/main.cpp`](../kern/tools/main.cpp) | Arguments, `runSource`, REPL, `kern test`, `kern doctor`, permission setup |
| **Lexer** | [`kern/core/compiler/lexer.cpp`](../kern/core/compiler/lexer.cpp), [`token.hpp`](../kern/core/compiler/token.hpp) | Tokens, keywords, literals |
| **Parser** | [`kern/core/compiler/parser.cpp`](../kern/core/compiler/parser.cpp), [`ast.hpp`](../kern/core/compiler/ast.hpp) | AST construction |
| **Semantic** | [`kern/core/compiler/semantic.cpp`](../kern/core/compiler/semantic.cpp) | Name resolution, preview strict paths |
| **Codegen** | [`kern/core/compiler/codegen.cpp`](../kern/core/compiler/codegen.cpp) | AST → [`Bytecode`](../kern/core/bytecode/bytecode.hpp) |
| **VM** | [`kern/runtime/vm/vm.cpp`](../kern/runtime/vm/vm.cpp), [`vm.hpp`](../kern/runtime/vm/vm.hpp) | Stack machine, dispatch, `VMError` |
| **Bytecode helpers** | [`kern/runtime/vm/bytecode_verifier.cpp`](../kern/runtime/vm/bytecode_verifier.cpp), [`bytecode_peephole.cpp`](../kern/core/bytecode/bytecode_peephole.cpp) | Safety checks, small optimizations |
| **Values** | [`kern/core/bytecode/value.hpp`](../kern/core/bytecode/value.hpp) | Language values (RC handles) |
| **Builtins** | [`kern/runtime/vm/builtins.hpp`](../kern/runtime/vm/builtins.hpp) | Native function table |
| **Imports** | [`src/import_resolution.cpp`](../src/import_resolution.cpp) | `__import`, module load |
| **Project** | [`kern/core/compiler/project_resolver.cpp`](../kern/core/compiler/project_resolver.cpp), [`analyzer/project_analyzer.cpp`](../kern/pipeline/analyzer/project_analyzer.cpp) | `kern.json`, static graph helpers |
| **Diagnostics** | [`kern/core/errors/errors.cpp`](../kern/core/errors/errors.cpp), [`errors.hpp`](../kern/core/errors/errors.hpp) | Human + JSON reporting |
| **Builtin module registry** | [`kern/modules/builtin_module_registry.hpp`](../kern/modules/builtin_module_registry.hpp) | **`get_builtin_modules()`** — names + future `init` hooks (Phase 10) |
| **Scanner tool** | [`src/scanner/`](../src/scanner/) | `kern --scan` registry checks |
| **LSP** | [`kern/tools/lsp_main.cpp`](../kern/tools/lsp_main.cpp) | Language server (when built) |
| **Graphics / game** | [`kern/modules/g2d/`](../kern/modules/g2d/), [`kern/modules/g3d/`](../kern/modules/g3d/), [`kern/modules/game/`](../kern/modules/game/) | Raylib-backed modules + `game` when `KERN_BUILD_GAME=ON` |

## Configuration and feature flags

- **CMake** options (e.g. `KERN_BUILD_GAME`, `KERN_WERROR`) control optional subsystems; see root [`CMakeLists.txt`](../CMakeLists.txt).
- **Runtime feature flags** for experimental compiler paths live under [`config/feature_flags.json`](../config/feature_flags.json) (referenced from roadmap docs).

## Binaries

| Binary | Purpose |
|--------|---------|
| **`kern`** | Primary interpreter / REPL / `kern test` |
| **`kernc`** | Compiler driver (batch compile; Windows launcher pattern in CMake) |
| **`kern-scan`** | Static checks without full execution |
| **`kern_lsp`** | LSP server (optional target) |

## See also

- [INTERNALS_COMPILER_AND_BYTECODE.md](INTERNALS_COMPILER_AND_BYTECODE.md)
- [INTERNALS_VM.md](INTERNALS_VM.md)
- [INTERNALS_MODULES_AND_SECURITY.md](INTERNALS_MODULES_AND_SECURITY.md)
