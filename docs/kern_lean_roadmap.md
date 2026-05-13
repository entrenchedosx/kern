# Kern Language Implementation: Completed Architecture

This document describes the completed implementation of the Kern programming language across all 10 development rungs. Each rung represents a feature set that was designed, implemented, and integrated into the compiler pipeline and virtual machine.

---

## Rung 1: Named Arguments

### Status: COMPLETED

Named arguments are supported at the parser level. Function call arguments can be specified as `name: value` pairs in any order. The parser maps named arguments to the corresponding parameter positions at parse time.

Implementation resides in [`kern/core/compiler/parser.cpp`](../kern/core/compiler/parser.cpp) within the call-expression parsing path. Named arguments are represented in the AST as [`CallArg`](../kern/core/compiler/ast.hpp:77) nodes with an optional `name` field.

---

## Rung 2: Random Function

### Status: COMPLETED

A `random()` built-in function provides deterministic pseudo-random number generation. Implemented as a native built-in within the VM's builtin registry at [`kern/runtime/vm/builtins.hpp`](../kern/runtime/vm/builtins.hpp).

---

## Rung 3: Enhanced For-Each (For-In)

### Status: COMPLETED

The `for` loop supports three forms:
- **Range-based**: `for i in 0..10 { ... }`
- **C-style**: `for i = 0; i < 10; i++ { ... }`
- **For-in**: `for item in collection { ... }`

For-in loops iterate over array elements. The loop variable receives each element in sequence. AST nodes are defined at [`kern/core/compiler/ast.hpp:407`](../kern/core/compiler/ast.hpp:407) ([`ForInStmt`](../kern/core/compiler/ast.hpp:407)), and code generation is in [`kern/core/compiler/codegen.cpp`](../kern/core/compiler/codegen.cpp).

---

## Rung 4: Vec3 Type

### Status: COMPLETED

The `vec3` type is a native VM value type stored in the [`Value`](../kern/core/bytecode/value.hpp:45) struct as `Type::VEC3`. It holds three `double` values (x, y, z) and is implemented in [`kern/core/bytecode/value.hpp:34`](../kern/core/bytecode/value.hpp:34) as [`Vec3Object`](../kern/core/bytecode/value.hpp:34).

Built-in operations are registered in [`kern/runtime/vm/vec3_builtins.hpp`](../kern/runtime/vm/vec3_builtins.hpp):

| Index | Function | Description |
|-------|----------|-------------|
| 0 | `vec3_new(x, y, z)` | Create a new vec3 |
| 1 | `vec3_add(a, b)` | Component-wise addition |
| 2 | `vec3_sub(a, b)` | Component-wise subtraction |
| 3 | `vec3_dot(a, b)` | Dot product |
| 4 | `vec3_normalize(v)` | Normalize to unit length |
| 5 | `struct_define(...)` | Struct definition builtin |
| 6 | `ok(value)` | Create a success Result |
| 7 | `err(msg)` | Create an error Result |
| 8 | `print(value)` | Print a value to stdout |

Vec3 fields are accessible via dot notation when a struct field is declared as `vec3` type, using the struct field layout metadata mechanism in [`vec3_builtins.hpp:14`](../kern/runtime/vm/vec3_builtins.hpp:14).

---

## Rung 5: Structs (No Generics)

### Status: COMPLETED

Structs are a compile-time desugaring feature. A struct declaration such as:

```kern
struct Entity { id: int, name: string, pos: vec3 }
```

is parsed into a [`StructDeclStmt`](../kern/core/compiler/ast.hpp:260) AST node. The code generator desugars struct instantiation into map construction, and field access (`entity.id`) into map index operations. This approach requires no runtime struct support -- the VM only works with maps (arrays of key-value pairs).

Struct field access supports UFCS, so `entity.get_id()` desugars to `get_id(entity)`, which calls a registered built-in or user-defined function with the struct instance as the first argument.

Implementation spans:
- **Parser**: [`kern/core/compiler/parser.cpp`](../kern/core/compiler/parser.cpp) -- struct declaration parsing
- **AST**: [`kern/core/compiler/ast.hpp:254-264`](../kern/core/compiler/ast.hpp:254) -- `StructFieldDecl`, `StructDeclStmt`
- **Codegen**: [`kern/core/compiler/codegen.cpp`](../kern/core/compiler/codegen.cpp) -- struct desugaring to maps
- **VM**: [`kern/runtime/vm/vec3_builtins.hpp:14`](../kern/runtime/vm/vec3_builtins.hpp:14) -- struct field layout metadata for vec3 integration

---

## Rung 6: Module System (File-Based)

### Status: COMPLETED

The module system supports file-based imports via the `import` statement. The parser recognizes `import "module_name"` syntax and resolves modules at compile time.

Module resolution uses a registry of known standard library modules defined in [`src/stdlib_modules.cpp`](../src/stdlib_modules.cpp). The [`ModuleRegistry`](../kern/runtime/vm/module_registry.cpp) manages module lifecycle at runtime.

The [`ImportStmt`](../kern/core/compiler/ast.hpp:457) AST node carries the module path, and the code generator emits initialization calls for imported modules.

---

## Rung 7: Result Type + `?` Operator

### Status: COMPLETED

The `Result<T, E>` type is a first-class C++ type defined in [`kern/core/value.hpp:116`](../kern/core/value.hpp:116):

```cpp
template<typename T, typename E = ErrorValue>
class Result {
    bool ok_;
    T value_;
    E error_;
public:
    T unwrap();
    bool isOk() const;
};
```

In Kern source code, `ok(value)` and `err(message)` are built-in functions registered at indices 6 and 7 of the vec3 builtin set. The `?` postfix operator is desugared at compile time:

```kern
let val = try_get()?;
// desugars to:
let _result = try_get();
if (!_result.is_ok()) { return err(_result.error()); }
let val = _result.unwrap();
```

The `?` operator is parsed as a [`TryExpr`](../kern/core/compiler/ast.hpp:211) in the AST and compiled to conditional early-return bytecode in the code generator.

Error values propagate through the VM's exception frame system ([`ExceptionFrame`](../kern/runtime/vm/vm.hpp:89)) and can be caught with `try`/`catch` blocks.

---

## Rung 8: Defer

### Status: COMPLETED

The `defer` statement queues a cleanup block for execution when the current scope exits. Deferred blocks execute in last-in-first-out (LIFO) order, matching the stack discipline of resource acquisition.

```kern
{
    let handle = open("file.txt");
    defer { close(handle); }
    // handle is automatically closed on scope exit
}
```

Implementation:
- **AST**: [`DeferStmt`](../kern/core/compiler/ast.hpp:363) node containing the deferred block
- **Codegen**: The code generator emits a `DEFER_START` marker and associates the deferred block's bytecode with the current scope. On scope exit (including during panic/error unwinding), the deferred blocks execute in reverse order.
- **VM**: The runtime maintains a deferred-call stack per call frame. On normal return or panic, deferred calls are unwound.

---

## Rung 9: Array Improvements (Collections)

### Status: COMPLETED

Dynamic arrays support the following built-in operations, registered in [`kern/runtime/vm/collection_builtins.hpp`](../kern/runtime/vm/collection_builtins.hpp):

| Index | Function | Description |
|-------|----------|-------------|
| 9 | `len(arr)` | Return the number of elements |
| 10 | `push(arr, val)` | Append an element |
| 11 | `pop(arr)` | Remove and return the last element |
| 12 | `remove(arr, idx)` | Remove element at index |

These builtins are registered after Vec3 builtins (which use indices 0-8) via the `startIndex` parameter:

```cpp
kern::registerCollectionBuiltins(vm3, 9);  // start after vec3 builtins
```

Array literals `[]` are parsed as [`ArrayLiteral`](../kern/core/compiler/ast.hpp:184) and compiled to `BUILD_ARRAY` bytecode instructions. UFCS allows `entities.push(item)` syntax to desugar to `push(entities, item)`.

---

## Rung 10: Grand Unification Integration Test

### Status: COMPLETED

The integration test in [`kern/cli/main.cpp`](../kern/cli/main.cpp) validates all subsystems working together:

**Test 1: Core Semantics**
Tests function calls, closures, if/else branches, pattern matching (match), defer execution ordering, and Result type unwrapping. Exercises the lexer, parser, code generator, peephole optimizer, bytecode verifier, and VM execution.

**Test 2: Struct Desugaring and UFCS**
Tests struct definition, field access via dot notation, UFCS method-call syntax on struct instances, and struct nesting. Validates that structs desugar correctly to maps and that field access compiles to map index operations.

**Test 3: Collections, Vec3, and Struct Integration**
Tests the integrated feature set:
- `vec3_new()` creating vec3 values in arrays
- `entities.push()` via UFCS desugaring
- `entities.len()` returning correct count
- `entities.pop()` returning the pushed entity
- Struct fields with `vec3` type
- Result-based error propagation with `ok()`
- Final `print("Success")` confirming all subsystems passed

All three tests execute sequentially in separate VM instances and produce validated output.

---

## Compiler Pipeline

```
Source Code
    |
    v
[Lexer] --> Token Stream
    |
    v
[Parser] --> AST (Abstract Syntax Tree)
    |
    v
[Semantic Analysis] --> Typed AST + Diagnostics
    |
    v
[Code Generator] --> Bytecode (Instruction Stream)
    |
    v
[Peephole Optimizer] --> Optimized Bytecode
    |
    v
[Bytecode Verifier] --> Verified Bytecode
    |
    v
[Virtual Machine] --> Execution Result
```

### Components

| Component | File | Description |
|-----------|------|-------------|
| Lexer | [`kern/core/compiler/lexer.cpp`](../kern/core/compiler/lexer.cpp) | Hand-written lexer producing token stream |
| Lexer Header | [`kern/core/compiler/lexer.hpp`](../kern/core/compiler/lexer.hpp) | Lexer class definition |
| Token Types | [`kern/core/compiler/token.hpp`](../kern/core/compiler/token.hpp) | Enum of all token types |
| Parser | [`kern/core/compiler/parser.cpp`](../kern/core/compiler/parser.cpp) | Recursive-descent parser |
| Parser Header | [`kern/core/compiler/parser.hpp`](../kern/core/compiler/parser.hpp) | Parser class definition |
| AST | [`kern/core/compiler/ast.hpp`](../kern/core/compiler/ast.hpp) | All AST node type definitions |
| Code Generator | [`kern/core/compiler/codegen.cpp`](../kern/core/compiler/codegen.cpp) | AST-to-bytecode compilation |
| Codegen Header | [`kern/core/compiler/codegen.hpp`](../kern/core/compiler/codegen.hpp) | CodeGenerator class definition |
| Bytecode | [`kern/core/bytecode/bytecode.hpp`](../kern/core/bytecode/bytecode.hpp) | Opcode and Instruction definitions |
| Value Types | [`kern/core/bytecode/value.hpp`](../kern/core/bytecode/value.hpp) | Runtime value representations |
| Value Header | [`kern/core/value.hpp`](../kern/core/value.hpp) | C++ value types (Result, SmallString) |
| Peephole Opt | [`kern/core/bytecode/bytecode_peephole.cpp`](../kern/core/bytecode/bytecode_peephole.cpp) | Local bytecode optimization passes |
| Verifier | [`kern/core/bytecode/bytecode_verifier.cpp`](../kern/core/bytecode/bytecode_verifier.cpp) | Bytecode correctness validation |
| VM | [`kern/runtime/vm/vm.cpp`](../kern/runtime/vm/vm.cpp) | Register-based bytecode interpreter |
| VM Header | [`kern/runtime/vm/vm.hpp`](../kern/runtime/vm/vm.hpp) | VM class with ~80 opcodes |
| Vec3 Builtins | [`kern/runtime/vm/vec3_builtins.hpp`](../kern/runtime/vm/vec3_builtins.hpp) | Vec3 math operations (indices 0-8) |
| Collection Builtins | [`kern/runtime/vm/collection_builtins.hpp`](../kern/runtime/vm/collection_builtins.hpp) | Array operations (indices 9-12) |
| Main Entry | [`kern/cli/main.cpp`](../kern/cli/main.cpp) | CLI entry point and integration tests |

---

## Virtual Machine Architecture

The VM is a register-based interpreter with the following characteristics:

- **Value Representation**: [`Value`](../kern/core/bytecode/value.hpp:45) is a tagged union supporting 14 types: NIL, BOOL, INT, FLOAT, STRING, ARRAY, MAP, FUNC, NATIVE, CLOSURE, GENERATOR, VEC3, STRUCT, CLASS, INSTANCE
- **Builtin Dispatch**: Built-in functions are stored in a `builtinsVec_` vector for O(1) indexed dispatch. The VM resolves builtins via instruction operands and calls them directly without hash map lookups.
- **Call Frames**: [`CallFrame`](../kern/runtime/vm/vm.hpp:78) structure for each function invocation, storing the instruction pointer, base register, and return address.
- **Exception Handling**: [`ExceptionFrame`](../kern/runtime/vm/vm.hpp:89) for try/catch unwinding with stack-allocated guard frames.
- **Deferred Execution**: Per-frame deferred call stacks, unwound on normal return and during panic propagation.
- **Runtime Guards**: [`RuntimeGuardPolicy`](../kern/runtime/vm/vm.hpp:32) and [`VMGuard`](../kern/runtime/vm/vm.hpp:393) for configurable safety constraints.
- **Memory Management**: Reference-counted [`ValuePtr`](../kern/core/bytecode/value.hpp) (shared_ptr) with optional pool allocator in the full builtins set.

---

## Built-in Function Index Map

To avoid index collisions between feature sets, the builtin registration uses explicit index offsets:

| Index Range | Feature | File |
|-------------|---------|------|
| 0 - 8 | Vec3 math, struct define, Result, print | [`vec3_builtins.hpp`](../kern/runtime/vm/vec3_builtins.hpp) |
| 9 - 12 | Collection operations (len, push, pop, remove) | [`collection_builtins.hpp`](../kern/runtime/vm/collection_builtins.hpp) |

The `registerCollectionBuiltins()` function accepts a `startIndex` parameter (default 0) to allow flexible placement after other builtin sets.
