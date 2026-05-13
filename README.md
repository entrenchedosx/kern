# Kern Programming Language

Kern is a **general-purpose scripting language** designed for everyday programming. It combines **Python-like ergonomics** -- clean indentation-based syntax, UFCS method calls, for-in loops, closures -- with **C++-level performance** via a custom bytecode Virtual Machine written in C++17.

Every language feature exists because it solves a real problem: Structs replace fragile dictionaries, native Vec3 eliminates dozens of global position variables, the `?` operator replaces nested error-checking, and `defer` automates resource cleanup. The result is a language that reads like a modern script but runs on a hand-tuned VM with zero external dependencies.

The entire compiler and VM are written in **C++17 with zero external dependencies**. You can build a **portable, standalone executable** from source in under 30 seconds with a single command -- no runtime DLLs required.

---

## Quick Look

Here is a complete game-loop snippet written in Kern. It uses **Structs** for data, **Vec3** for position math, **UFCS** (Uniform Function Call Syntax) for readable method calls, **Collections** for entity storage, the **`?` operator** for safe error propagation, and **`defer`** for automatic cleanup:

```python
struct Entity {
    id: int,
    pos: vec3,
    hp: int,
    tag: string
}

def run_frame() {
    let entities = [];
    defer { print("frame complete"); }

    let v = vec3_new(1.0, 2.0, 3.0);
    entities.push(Entity(id: 1, pos: v, hp: 100, tag: "player"));

    for entity in entities {
        print(entity.hp);
    }

    let last = entities.pop()?;
    return ok(last);
}

let result = run_frame();
match result {
    ok(val) => print("Entity: " + val.tag),
    err(e)  => print("Error: " + e)
}
```

This demonstrates:
- **Structs** with named, typed fields (no dictionaries)
- **UFCS**: `entities.push(x)` reads like OOP but desugars to `push(entities, x)`
- **Vec3**: native `vec3_new()`, `vec3_add()`, `vec3_dot()` for 3D math
- **Collections**: `push`, `pop`, `len`, `remove` on dynamic arrays
- **`?` operator**: `pop()?` returns the value or early-exits on error
- **`defer`**: runs code when the scope exits, even during errors
- **`match`**: pattern matching on Result types

---

## Why Kern Matters for Game Development

### From Pain Points to Language Features

Kern's feature set was directly motivated by real friction in game scripting. Every language construct exists because it solves a problem that game developers face daily.

| The Problem | The Kern Solution |
|---|---|
| Dictionary-based entities with string keys everywhere | **Structs** with named, typed fields and dot access |
| Dozens of global `playerX`, `playerY`, `bulletX`, `bulletY` variables | **Native Vec3 type** for all position, velocity, and collision math |
| Verbose `push(array, x)` calls that bury readability | **UFCS** lets you write `array.push(x)` -- it desugars at compile time |
| Manual index tracking in every loop | **For-in loops**: `for item in collection { ... }` |
| inline collision math with raw dx/dy | **Vec3 built-ins**: `vec3_add()`, `vec3_sub()`, `vec3_dot()`, `vec3_normalize()` |
| Error codes and nested if-checks everywhere | **Result type + `?` operator**: `let val = risky_call()?` |
| Manual resource cleanup on every early exit | **Defer**: `defer { cleanup() }` runs automatically on scope exit |

---

## Features (Beginner-Friendly)

### Structs -- Real Data Types, Not Dictionaries

Declare data with named fields and type annotations. Access them with dot notation. No string keys, no typos, no runtime surprises.

```python
struct Player { name: string, pos: vec3, health: int }
let p = Player(name: "hero", pos: vec3_new(0, 0, 0), health: 100);
print(p.name);  # "hero"
```

### Native Vec3 Math -- Built for 3D

Vec3 is a first-class VM type, not a library. Operations run directly in the bytecode interpreter without boxing overhead.

```python
let a = vec3_new(1.0, 0.0, 0.0);
let b = vec3_new(0.0, 1.0, 0.0);
let sum = vec3_add(a, b);       # (1, 1, 0)
let dot = vec3_dot(a, b);       # 0.0
let n = vec3_normalize(sum);    # unit vector
```

### Clean Object-Oriented Syntax (UFCS)

Write `array.push(x)` and `list.len()` without classes or inheritance. The compiler transparently desugars method-call syntax into free-function calls at compile time. Zero runtime cost.

```python
let items = [];
items.push("sword");
items.push("shield");
print(items.len());    # 2
let top = items.pop();
```

### Rust-Style Error Handling with `?`

The `Result<T>` type and the postfix `?` operator let you write safe, linear code without nested try/catch blocks. Create success values with `ok(value)` and errors with `err(message)`.

```python
def divide(a, b) {
    if b == 0 { return err("cannot divide by zero"); }
    return ok(a / b);
}

def compute() {
    let val = divide(10, 2)?;   # unwraps or early-returns the error
    return ok(val * 2);
}
```

### Defer -- Automatic Cleanup

Schedule cleanup code to run when the current scope exits, regardless of how it exits (normal return, error, or panic). Deferred blocks execute in reverse order (LIFO).

```python
def process_file() {
    let handle = open("data.txt");
    defer { close(handle); }
    # handle is automatically closed on scope exit
    # even if an error occurs
}
```

### Built-in Collections

Dynamic arrays with four operations registered directly in the VM:

| Operation | Description |
|---|---|
| `len(arr)` | Return the number of elements |
| `push(arr, val)` | Append an element |
| `pop(arr)` | Remove and return the last element |
| `remove(arr, idx)` | Remove element at a specific index |

### Control Flow

`if`/`else`, `while`, `for` (range, C-style, and for-in), `repeat`, `repeat-while`, `match` (pattern matching), `break`, `continue`, `try`/`catch`, `throw`, `assert`.

### Closures and Lambdas

Anonymous functions with capture-by-reference semantics. Nested function declarations with full closure environment support.

---

## Getting Started

### Prerequisites

- **Windows** with **MSYS2 MinGW-w64** installed (g++ 13+)
  - Download from https://www.msys2.org
  - The compiler will be at `C:\msys64\mingw64\bin\g++.exe`

### Build from Source (One Command)

Open **PowerShell** or **Command Prompt** and run:

```powershell
set "PATH=C:\msys64\mingw64\bin;%PATH%" ^
&& g++ -std=c++17 -O3 -I. -Ikern -Ikern/core -Ikern/runtime/core -Ikern/runtime/bindings ^
  kern/core/compiler/lexer.cpp kern/core/compiler/parser.cpp kern/core/compiler/codegen.cpp ^
  kern/core/bytecode/bytecode_peephole.cpp kern/core/bytecode/bytecode_verifier.cpp ^
  kern/core/value.cpp kern/runtime/vm/vm.cpp kern/runtime/vm/http_get_winhttp.cpp ^
  kern/runtime/core/scheduler.cpp kern/runtime/bindings/native_bindings.cpp ^
  kern/runtime/core/module_registry.cpp kern/cli/main.cpp ^
  -o kern.exe -lwinhttp -static-libgcc -static-libstdc++ -static
```

This compiles all 13 source files with full optimization (`-O3`), statically links the MinGW runtime (no external DLLs needed), and produces a **portable `kern.exe`** that runs on any Windows machine. Build completes in 10-30 seconds.

### Run the Integration Test

```powershell
set "PATH=C:\msys64\mingw64\bin;%PATH%" && kern.exe
```

You will see the Grand Unification Integration Test execute all three test suites:

```
=== Struct V1 Test ===
e.y =
25.000000

=== Result ? Operator Test ===
Div by zero

=== Grand Unification Test ===
<vec3>
1
Success
```

---

## Project Structure

```
kern/
  core/
    compiler/       -- Lexer, Parser, AST, Code Generator
    bytecode/       -- Opcodes, Value types, Peephole optimizer
  runtime/
    vm/             -- Virtual machine, Built-in functions
    core/           -- Scheduler, Module registry
    bindings/       -- Native (FFI) binding layer
  cli/main.cpp      -- Entry point
docs/               -- Architecture and design documentation
examples/           -- Kern source code examples
Kern-IDE/           -- Python-based IDE
kern-registry/      -- Package registry
kern-bootstrap/     -- Rust-based bootstrap installer
```

---

## License

See [LICENSE](LICENSE) for details.
