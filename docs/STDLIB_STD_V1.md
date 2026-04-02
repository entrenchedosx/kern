# Kern `std.v1` built-in modules

Kern exposes a **versioned** standard module namespace: import maps returned by `__import` (no `.kn` file).

## Principles

- **New native APIs** added for the standard library use **internal builtin names** `std_*` (registered in the VM, **not** added as new globals).
- User-facing names live on **`std.v1.*` module maps** (short keys like `fmod`, `index_of`).
- **Legacy** flat modules (`import("math")`, `import("string")`, …) are unchanged.
- **Aliases**: `import("std.math")` resolves to the same surface as `import("std.v1.math")`.

## Hierarchy

| Import | Contents |
|--------|----------|
| `import("std")` | Map with key `v1` → nested bundle |
| `import("std.v1")` | Map of submodules: `math`, `string`, `bytes`, `collections`, `fs`, `process`, `time` |
| `import("std.v1.math")` | New math builtins + `PI`, `E`, `TAU` |
| `import("std.v1.string")` | String helpers |
| `import("std.v1.bytes")` | Byte-array helpers (arrays of ints 0–255) |
| `import("std.v1.collections")` | Array/map helpers |
| `import("std.v1.fs")` | Re-exports of existing file/path builtins |
| `import("std.v1.process")` | Re-exports of process/env builtins |
| `import("std.v1.time")` | Re-exports of time builtins |

## Example

```text
let m = import("std.v1.math")
print(m["fmod"](5.0, 2.0))
```

## Future versions

`std.v2.*` can be introduced later without breaking `std.v1.*` imports.
