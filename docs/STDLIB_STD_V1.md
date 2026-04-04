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
| `import("std.v1")` | Map of submodules: `math`, `string`, `bytes`, `collections`, `fs`, `process`, `net`, `os`, `signal`, `memory`, `task`, `sync`, `time` |
| `import("std.v1.math")` | New math builtins + `PI`, `E`, `TAU` |
| `import("std.v1.string")` | String helpers |
| `import("std.v1.bytes")` | Byte-array helpers (arrays of ints 0–255) |
| `import("std.v1.collections")` | Array/map helpers |
| `import("std.v1.fs")` | Re-exports of existing file/path builtins |
| `import("std.v1.process")` | Re-exports of process/env builtins |
| `import("std.v1.net")` | HTTP + URL + TCP/UDP socket primitives |
| `import("std.v1.os")` | Platform/runtime metadata and host helpers |
| `import("std.v1.signal")` | Process cancellation/signal helpers |
| `import("std.v1.memory")` | Low-level memory/map-file primitives |
| `import("std.v1.task")` | Cooperative task helpers (`spawn`/`await`) |
| `import("std.v1.sync")` | Retry/cleanup synchronization utilities |
| `import("std.v1.time")` | Re-exports of time builtins |

## Example

```text
let m = import("std.v1.math")
print(m["fmod"](5.0, 2.0))
```

## Future versions

`std.v2.*` can be introduced later without breaking `std.v1.*` imports.

## Security model notes

- Privileged calls remain **secure-by-default** and permission-gated.
- Permission groups can be granted via CLI/runtime APIs: `fs.readonly`, `fs.readwrite`, `net.client`, `proc.control`, `env.manage`.
- FFI remains explicit: requires runtime `ffi` enablement and unsafe context (`ffi_call` / `ffi_call_typed`).
