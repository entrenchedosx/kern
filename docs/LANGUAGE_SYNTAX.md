# Language syntax overview

This is a **high-level** overview. Kern evolves; the **compiler** (`kern/core/compiler/`) and **examples** are the ground truth.

## Basics

- **Statements:** `let`, `var`, `const`; `def` for functions; `class` for types; `return`, `if` / `else`, `while`, `for`, `match`, `try` / `catch` / `finally`, `throw`, `defer`.
- **Values:** numbers, strings, booleans, `nil`, arrays `[a, b]`, maps `{ "k": v }`.
- **Calls:** `f(a, b)`; methods `obj.method(x)` where supported.
- **Modules:** `import("math")`, `import("lib/kern/foo")` — resolution depends on `KERN_LIB` and working directory (see [TROUBLESHOOTING](TROUBLESHOOTING.md)).

## Operators (non-exhaustive)

- Arithmetic: `+`, `-`, `*`, `/`, `%`
- Comparison: `==`, `!=`, `<`, `<=`, `>`, `>=`
- Logic: `&&`, `||`, `!`
- Optional chaining: `?.`
- Null coalescing: `??`

## Examples in-repo

- **Hello / basics:** [`examples/basic/`](../examples/basic/)
- **Larger smoke:** [`examples/advanced/`](../examples/advanced/)

For **native stdlib modules** (`std.v1.math`, …), see [STDLIB_STD_V1.md](STDLIB_STD_V1.md).
