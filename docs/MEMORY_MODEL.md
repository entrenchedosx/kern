# Kern memory model

Kern is implemented as a **C++ VM** with **reference-counted** `Value` objects (`std::shared_ptr<Value>`). User code does not manually allocate or free heap memory; there is **no** exposed garbage collector or `free()` for ordinary values.

## Values

- **Numbers, booleans, null** are stored inline.
- **Strings, arrays, maps, functions, and errors** live on the heap; lifetimes are managed by **shared ownership**.
- When nothing references a heap value, it is destroyed automatically (deterministic at last reference drop).

## Tables and mutation

- **Map and array** updates mutate in place when the value is shared; callers holding references see updates unless they copy explicitly using APIs such as `copy()` where provided by the standard library.

## FFI and unsafe blocks

- Raw memory (`alloc`, `peek64`, etc.) is available only inside **`unsafe { }`** or with explicit runtime flags. That memory is **not** managed by the Kern value heap; callers must pair `alloc`/`free` correctly.

## Concurrency

- The VM is **one thread per process** for normal execution. Builtins such as **`spawn`** / OS **`process.spawn`** may create separate threads or processes; **do not** share mutable Kern `Value` handles across threads unless documented for that API.
- **Cooperative tasks** (`async def`, `spawn expr`, `await expr`) run on the **same VM** unless a builtin explicitly uses a different execution context.

## Summary

Kern uses **automatic reference counting** for language values, **no tracing GC** in the current implementation, and **no manual memory management** for ordinary code paths.
