# Kern Internals Architecture

## High-Level Pipeline

```
Source (.kn)  -->  Lexer  -->  Parser  -->  Code Generator  -->  Peephole Optimizer  -->  Verifier  -->  VM
```

All passes are hand-written in C++17 with zero external dependencies (except the Windows FFI backend which links `winhttp.dll`).

## Top-Level Source Map

### Compiler Frontend (`kern/core/`)

| Path | Purpose |
|------|---------|
| `compiler/lexer.cpp` / `.hpp` | Hand-written lexer: character stream to token stream |
| `compiler/token.hpp` | Token type enum (~60 token types) |
| `compiler/parser.cpp` / `.hpp` | Recursive-descent parser: token stream to AST |
| `compiler/ast.hpp` | All AST node definitions (~50 node types) |
| `compiler/codegen.cpp` / `.hpp` | AST-to-bytecode compiler with UFCS desugaring, struct desugaring, `?` operator desugaring |
| `compiler/semantic.hpp` | Semantic analysis diagnostics and severity levels |
| `bytecode/bytecode.hpp` | Opcode enum (~80 opcodes), Instruction struct with format-string display |
| `bytecode/bytecode_peephole.cpp` | Local peephole optimization passes over bytecode |
| `bytecode/bytecode_verifier.cpp` | Bytecode correctness validation (CFG, type stacks, operand bounds) |
| `bytecode/value.hpp` | Runtime Value tagged union (14 types including VEC3, STRUCT) |
| `value.hpp` | C++ helper types (Result<T,E>, SmallString, ErrorValue) |
| `errors/vm_error_codes.hpp` | VM error code enum |

### VM Runtime (`kern/runtime/vm/`)

| Path | Purpose |
|------|---------|
| `vm.cpp` / `vm.hpp` | Register-based bytecode interpreter. Each instruction decoded via a large switch statement. Manages call frames, exception frames, deferred execution stacks, register windows, and builtin dispatch. |
| `builtins.hpp` | Full set of ~500+ built-in functions including JSON, HTTP, crypto, filesystem, process, base64, URL utilities |
| `vec3_builtins.hpp` | Vec3 math builtins (indices 0-8): new, add, sub, dot, normalize, struct_define, ok, err, print |
| `collection_builtins.hpp` | Collection builtins (indices 9-12): len, push, pop, remove |
| `scheduler.cpp` | Cooperative coroutine scheduler for async/await support |
| `http_get_winhttp.cpp` | Windows HTTP client via WinHTTP API |
| `native_bindings.cpp` | FFI bridge for calling native C functions |
| `module_registry.cpp` | Module lifecycle management for imported modules |

### Entry Point

| Path | Purpose |
|------|---------|
| `cli/main.cpp` | CLI entry point. Generates three test programs, compiles each through the full pipeline, and executes them on separate VM instances. |

## Configuration and Feature Flags

The `VMConfig` struct in [`vm.hpp:103`](../kern/runtime/vm/vm.hpp:103) provides runtime configuration:
- **Execution limits**: `maxSteps_`, `maxInstructions_`, `maxMemory_` to bound resource usage
- **Guard policies**: [`RuntimeGuardPolicy`](../kern/runtime/vm/vm.hpp:32) enables/disables bytecode verification, peephole optimization, and instruction limits
- **Callback guards**: [`VMGuard`](../kern/runtime/vm/vm.hpp:393) provides RAII-scoped step and recursion limits for foreign callbacks

## Builtin Dispatch Mechanism

1. Builtins are registered via `VM::registerBuiltin(index, fn)` which stores them in both an `unordered_map<size_t, BuiltinFn>` and a `vector<BuiltinFn>`.
2. At instruction execution time, if the function's `builtinIndex` is within the vector bounds, dispatch is O(1) vector lookup.
3. Vec3 builtins occupy indices 0-8. Collection builtins accept a `startIndex` parameter (default 0) to avoid index collisions when multiple builtin sets are registered on the same VM.

## Value System

The [`Value`](../kern/core/bytecode/value.hpp:45) struct is a tagged union supporting these types:

NIL, BOOL, INT, FLOAT, STRING, ARRAY, MAP, FUNC, NATIVE, CLOSURE, GENERATOR, VEC3, STRUCT, CLASS, INSTANCE

Values are managed via `ValuePtr` (aliased `std::shared_ptr<Value>` or raw `Value*` depending on the compilation unit). The full builtins set in `builtins.hpp` includes a pool allocator (`MemoryManager`) for cache-friendly allocation with quarantine and deferred cleanup.

## See Also

- [`README.md`](../README.md) -- Build instructions and feature overview
- [`docs/kern_lean_roadmap.md`](kern_lean_roadmap.md) -- Detailed feature implementation with component map
- [`docs/kern_evolution_from_stardefender.md`](kern_evolution_from_stardefender.md) -- Feature origins and motivation
