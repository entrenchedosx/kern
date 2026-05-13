# Kern Language Deep Refactoring Plan
**Version:** 2.0.2  
**Status:** Phase 2 Complete

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                    FRONTEND (Compiler)                       │
├─────────────────────────────────────────────────────────────┤
│  Source → Lexer → Parser → AST → IR → Optimizer → Bytecode   │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                    RUNTIME (VM)                              │
├─────────────────────────────────────────────────────────────┤
│  Bytecode Loader → Verifier → JIT (opt) → Execute          │
│  Stack Machine with Register Window                          │
│  Arena Allocator for Values                                  │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                    MODULES (Isolated)                        │
├─────────────────────────────────────────────────────────────┤
│  Core (math, io, string) | Graphics (g2d, g3d) | FFI        │
│  Each: Clean C API, no VM internals access                   │
└─────────────────────────────────────────────────────────────┘
```

## Key Changes

### 1. Value System Redesign
- Replace `shared_ptr<Value>` with `Value` (variant with small-string optimization)
- Arena allocation for bulk value allocation/deallocation
- Move semantics everywhere

### 2. VM Redesign
- Register-window stack machine (hybrid)
- Direct-threaded dispatch (computed goto)
- Instruction cache friendly layout

### 3. Memory Management
- Arena allocator for compilation phase
- Generational allocator for runtime
- Pool allocator for small objects

### 4. Module System
- Clean C API boundary
- No direct VM state access
- Explicit capability model

### 5. Error Handling
- Result<T, E> type for recoverable errors
- Exceptions only for fatal conditions
- Structured error codes

## File Restructuring

```
kern/
├── frontend/           # Lexer, Parser, AST
│   ├── lexer.cpp
│   ├── parser.cpp
│   ├── ast.cpp
│   └── ast.hpp
├── ir/                # Intermediate Representation
│   ├── ir.hpp
│   ├── builder.cpp
│   ├── passes/
│   │   ├── constant_fold.cpp
│   │   ├── dead_code.cpp
│   │   └── inline.cpp
│   └── codegen.cpp    # IR → Bytecode
├── runtime/           # VM and execution
│   ├── vm.cpp
│   ├── vm.hpp
│   ├── value.cpp      # New value system
│   ├── value.hpp
│   ├── allocator.cpp  # Arena/pool allocators
│   ├── allocator.hpp
│   └── bytecode_loader.cpp
├── modules/           # Isolated modules
│   ├── core/
│   ├── graphics/
│   │   ├── g2d.cpp    # Clean C++ API
│   │   ├── g3d.cpp
│   │   └── renderer.h # C interface
│   └── ffi/
├── core/              # Shared utilities
│   ├── result.hpp
│   ├── span.hpp
│   └── string_intern.hpp
└── tools/
    └── main.cpp
```

## Implementation Priority

1. Value system (critical path)
2. VM core with new dispatch
3. Allocator infrastructure
4. Module isolation
5. Graphics cleanup
6. Optimizations

## Breaking Changes

- `ValuePtr` → `Value` (copy instead of shared_ptr)
- Module API changes (explicit context parameter)
- Error handling (Result type)
- Bytecode format (version bump)
