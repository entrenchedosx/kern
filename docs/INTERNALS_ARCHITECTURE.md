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
| **Driver / CLI** | [`src/main.cpp`](../src/main.cpp) | Arguments, `runSource`, REPL, `kern test`, `kern doctor`, permission setup |
| **Lexer** | [`src/compiler/lexer.cpp`](../src/compiler/lexer.cpp), [`token.hpp`](../src/compiler/token.hpp) | Tokens, keywords, literals |
| **Parser** | [`src/compiler/parser.cpp`](../src/compiler/parser.cpp), [`ast.hpp`](../src/compiler/ast.hpp) | AST construction |
| **Semantic** | [`src/compiler/semantic.cpp`](../src/compiler/semantic.cpp) | Name resolution, preview strict paths |
| **Codegen** | [`src/compiler/codegen.cpp`](../src/compiler/codegen.cpp) | AST → [`Bytecode`](../src/vm/bytecode.hpp) |
| **VM** | [`src/vm/vm.cpp`](../src/vm/vm.cpp), [`vm.hpp`](../src/vm/vm.hpp) | Stack machine, dispatch, `VMError` |
| **Bytecode helpers** | [`src/vm/bytecode_verifier.cpp`](../src/vm/bytecode_verifier.cpp), [`bytecode_peephole.cpp`](../src/vm/bytecode_peephole.cpp) | Safety checks, small optimizations |
| **Values** | [`src/vm/value.hpp`](../src/vm/value.hpp) | Language values (RC handles) |
| **Builtins** | [`src/vm/builtins.hpp`](../src/vm/builtins.hpp) | Native function table |
| **Imports** | [`src/import_resolution.cpp`](../src/import_resolution.cpp) | `__import`, module load |
| **Project** | [`src/compiler/project_resolver.cpp`](../src/compiler/project_resolver.cpp), [`analyzer/project_analyzer.cpp`](../src/analyzer/project_analyzer.cpp) | `kern.json`, static graph helpers |
| **Diagnostics** | [`src/errors.cpp`](../src/errors.cpp), [`errors.hpp`](../src/errors.hpp) | Human + JSON reporting |
| **Scanner tool** | [`src/scanner/`](../src/scanner/) | `kern --scan` registry checks |
| **LSP** | [`src/lsp/lsp_main.cpp`](../src/lsp/lsp_main.cpp) | Language server (when built) |
| **Graphics** | [`src/modules/g2d/`](../src/modules/g2d/), [`src/modules/g3d/`](../src/modules/g3d/), [`src/game/`](../src/game/) | Raylib-backed modules when `KERN_BUILD_GAME=ON` |

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
