# Kern Programming Language

Kern is an experimental systems-programming language with a custom bytecode virtual machine. It combines C-like syntax with modern features such as Uniform Function Call Syntax (UFCS), compile-time struct desugaring, first-class Result types, defer statements, and built-in vector math.

The compiler pipeline consists of a hand-written lexer, recursive-descent parser, AST-to-bytecode code generator, peephole optimizer, and bytecode verifier -- all implemented in C++17 with no external dependencies.

## Features

### Completed (v2.1.0)

- **Structs** -- Named field aggregates with type annotations. Desugared at compile time into indexed maps. Supports field access via dot notation, struct literals in expressions, and nesting.
- **Uniform Function Call Syntax (UFCS)** -- Method-call syntax (`obj.method(args)`) transparently desugars to free-function calls (`method(obj, args)`) at compile time. Works with all user-defined and built-in functions.
- **Defer Statements** -- Stack-ordered deferred execution for resource cleanup. Pushes cleanup callbacks at runtime and executes them in reverse order on scope exit, including during panic unwinding.
- **Result Type with `?` Operator** -- First-class `Result<T, E>` type with `ok(value)` and `err(message)` constructors. The postfix `?` operator unwraps a Result or early-returns an error. Integrated with the runtime error system.
- **Collections** -- Dynamic arrays with `push`, `pop`, `len`, and `remove` built-in operations. Array literal syntax `[]` with type inference.
- **Vec3 Math** -- Native 3D vector type with `vec3_new`, `add`, `sub`, `dot`, `normalize` built-in operations. Struct integration: fields can be declared as `vec3`, allowing `entity.pos.x` access patterns.
- **Closures and Lambdas** -- Anonymous functions with capture-by-reference semantics. Nested function declarations with closure environments.
- **Control Flow** -- `if`/`else`, `while`, `for` (range, C-style, and for-in), `repeat`, `repeat-while`, `match`, `break`, `continue`.
- **Error Handling** -- `try`/`catch` blocks, `throw` for error propagation, `assert` for invariants.
- **Modules and Imports** -- File-based module system with `import` statements. Standard library module registry with aliased exports.
- **Bytecode VM** -- Custom register-based VM with ~80 opcodes. Includes peephole optimizer, bytecode verifier, runtime guard policies, and structured error messages.
- **Foreign Function Interface (FFI)** -- `ffi` declarations for calling native C functions on Windows via `LoadLibrary`/`GetProcAddress`.

### In Development

- Async/await with generator coroutine support
- Decorators and compile-time reflection
- Package manager (kargo) with registry
- GPU compute via WebGPU bindings
- Iterator protocol with for-in integration

## Build Instructions

### Prerequisites

- **MSYS2 MinGW-w64** (g++ 13+ or 16+)
  - Install from https://www.msys2.org
  - Ensure `C:\msys64\mingw64\bin` is on your PATH
- **Windows SDK** (for `winhttp.dll` linkage)

### Quick Build

```batch
set "PATH=C:\msys64\mingw64\bin;%PATH%"
g++ -std=c++17 -O3 -I. -Ikern ^
  kern/core/compiler/lexer.cpp ^
  kern/core/compiler/parser.cpp ^
  kern/core/compiler/codegen.cpp ^
  kern/core/bytecode/bytecode_peephole.cpp ^
  kern/core/bytecode/bytecode_verifier.cpp ^
  kern/core/bytecode/value.cpp ^
  kern/core/value.cpp ^
  kern/runtime/vm/vm.cpp ^
  kern/runtime/vm/scheduler.cpp ^
  kern/runtime/vm/http_get_winhttp.cpp ^
  kern/runtime/vm/native_bindings.cpp ^
  kern/runtime/vm/module_registry.cpp ^
  kern/cli/main.cpp ^
  -o kern.exe -lwinhttp
```

### Step-by-Step (Object Files)

```batch
set "PATH=C:\msys64\mingw64\bin;%PATH%"
g++ -std=c++17 -Wall -Wextra -O3 -c -I. -Ikern kern/core/compiler/lexer.cpp
g++ -std=c++17 -Wall -Wextra -O3 -c -I. -Ikern kern/core/compiler/parser.cpp
g++ -std=c++17 -Wall -Wextra -O3 -c -I. -Ikern kern/core/compiler/codegen.cpp
g++ -std=c++17 -Wall -Wextra -O3 -c -I. -Ikern kern/core/bytecode/bytecode_peephole.cpp
g++ -std=c++17 -Wall -Wextra -O3 -c -I. -Ikern kern/core/bytecode/bytecode_verifier.cpp
g++ -std=c++17 -Wall -Wextra -O3 -c -I. -Ikern kern/core/bytecode/value.cpp
g++ -std=c++17 -Wall -Wextra -O3 -c -I. -Ikern kern/core/value.cpp
g++ -std=c++17 -Wall -Wextra -O3 -c -I. -Ikern kern/runtime/vm/vm.cpp
g++ -std=c++17 -Wall -Wextra -O3 -c -I. -Ikern kern/runtime/vm/scheduler.cpp
g++ -std=c++17 -Wall -Wextra -O3 -c -I. -Ikern kern/runtime/vm/http_get_winhttp.cpp
g++ -std=c++17 -Wall -Wextra -O3 -c -I. -Ikern kern/runtime/vm/native_bindings.cpp
g++ -std=c++17 -Wall -Wextra -O3 -c -I. -Ikern kern/runtime/vm/module_registry.cpp
g++ -std=c++17 -Wall -Wextra -O3 -c -I. -Ikern kern/cli/main.cpp
g++ lexer.o parser.o codegen.o bytecode_peephole.o bytecode_verifier.o ^
  value.o vm.o scheduler.o http_get_winhttp.o native_bindings.o ^
  module_registry.o main.o -o kern.exe -lwinhttp
```

### Run

```batch
kern.exe
```

The program executes a Grand Unification Integration Test that validates all core subsystems.

## Project Structure

```
kern/
  core/
    compiler/       -- Lexer, Parser, AST, Code Generator, Semantic Analysis
    bytecode/       -- Bytecode definitions, Value types, Peephole optimizer, Verifier
    errors/         -- Error code definitions
  runtime/
    vm/             -- Virtual machine, Built-in functions, Scheduler, FFI bridge
  cli/
    main.cpp        -- Entry point and integration tests
cli/                -- Command-line interface entry point
docs/               -- Architecture and design documentation
tests/              -- Test suites
kargo/              -- Package manager (kargo)
Kern-IDE/           -- Python-based IDE
kern-registry/      -- Package registry server and metadata
kern-bootstrap/     -- Rust-based bootstrap installer
kern-portable-bootstrap/ -- Portable bootstrap variant
examples/           -- Kern source examples
3dengine/           -- 3D engine demo projects
```

## Syntax Overview

```kern
// Struct definition
struct Entity {
    id: int,
    pos: vec3,
    tag: string
}

// UFCS: method syntax desugars to function call
let entities = [];
entities.push(Entity(id: 1, pos: vec3_new(0.0, 0.0, 0.0), tag: "player"));

// Result type with ? operator
fn try_get(id: int) -> Result<Entity> {
    if id > 0 {
        return ok(Entity(id: id, ...));
    }
    return err("invalid id");
}

let entity = try_get(1)?;

// Defer for cleanup
fn process() {
    let handle = open_resource();
    defer { close_resource(handle); }
    // handle is automatically closed on scope exit
}

// Vec3 math
let v = vec3_add(vec3_new(1.0, 0.0, 0.0), vec3_new(0.0, 1.0, 0.0));
let d = vec3_dot(v, v);
```

## Testing

The integration test in `kern/cli/main.cpp` validates all subsystems:

1. **Core semantics** -- function calls, closures, if/else, pattern matching, defer, Result
2. **Struct desugaring** -- struct definition, field access, UFCS method calls on struct instances
3. **Collection + Vec3 integration** -- arrays, UFCS push/pop/len, vec3 operations, struct fields with vec3 type

## License

See LICENSE file for details.
