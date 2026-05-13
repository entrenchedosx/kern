# Builtin reference (where things live)

Kern exposes **hundreds** of native builtins. A single flat list in docs would go stale quickly. Use this page as a **map** to the source of truth.

## Canonical registry (C++)

- **Ordered names:** `getBuiltinNames()` in [`kern/runtime/vm/builtins.hpp`](../kern/runtime/vm/builtins.hpp) — this list defines **stable indices**. New builtins are appended; **never reorder** existing entries.
- **Implementations:** `registerAllBuiltins()` in the same file registers each index with a handler. Many builtins also get a **global** name via `setGlobalFn()`; newer `std_*` builtins may be **module-only** (no global).
- **Extra global aliases** (not in the main list order): `getBuiltinExtraGlobalNames()` in `builtins.hpp` (e.g. `readFile`, `PI`).

## Versioned stdlib modules (`std.v1.*`)

User-facing **short names** (e.g. `fmod`, `crc32`) are mapped to internal builtin names in:

- [`src/stdlib_stdv1_exports.hpp`](../src/stdlib_stdv1_exports.hpp) — `stdV1NamedExports()`
- [`src/stdlib_modules.cpp`](../src/stdlib_modules.cpp) — `createStdlibModule()`, legacy `MODULES` table

See [STDLIB_STD_V1.md](STDLIB_STD_V1.md) for how to import and call these modules.

## Static validation

- **`kern --scan --registry-only`** — checks duplicate / empty names in `getBuiltinNames()` and that every index has a VM registration.
- **`kern --scan`** — also checks stdlib export targets and scans `.kn` files (see [KERN_SCAN.md](KERN_SCAN.md)).

## Generated / packaged docs

For a **full** printable list of global names, the most reliable approach is to grep `setGlobalFn(` in `builtins.hpp` and merge with `getBuiltinNames()` / stdlib module tables above.
