# Kern vNext system access (implementation notes)

This document tracks the implemented vNext expansion work toward a Python+CPP blend:
simple surface APIs with stronger system reach, while remaining secure-by-default.

## Implemented feature slices

1. **`std.v1.net`** module exports HTTP + URL + TCP/UDP/socket-select builtins.
2. **`std.v1.process` v2 aliases**: `run` and `capture` mapped to argv/capture process builtins.
3. **`std.v1.fs` low-level extensions**: `map_file`, `unmap_file`, `memory_protect`.
4. **`std.v1.os`** module with runtime/platform/process metadata.
5. **`std.v1.signal`** minimal process-cancel surface via `kill_process`.
6. **Typed FFI wrapper**: `ffi_call_typed(descriptor[, args])`.
7. **`std.v1.memory`** module exports pointer/map-file primitives.
8. **Permissions v2 groups** in runtime/CLI/require flow.
9. **CLI capability profiles**: `kern capability profile [list|show|apply]`.
10. **Structured error helper**: `error_structured(error[, cause])`.
11. **`std.v1.task` + `std.v1.sync`** namespaces for cooperative task/sync helpers.
12. **LSP hover permission hints** for privileged/unsafe builtins.

## Permission groups and profiles

### Groups

- `fs.readonly` -> `filesystem.read`
- `fs.readwrite` -> `filesystem.read`, `filesystem.write`
- `net.client` -> `network.http`, `network.tcp`, `network.udp`
- `proc.control` -> `process.control`, `system.exec`
- `env.manage` -> `env.access`
- `system.full` -> all standard permissions

### Profiles

- `secure` -> no pre-grants
- `dev` -> `fs.readwrite`, `net.client`, `proc.control`, `env.manage`
- `ci` -> `fs.readwrite`, `net.client`, `proc.control`

## Test matrix expansion (recommended)

1. **Permission deny/allow**
   - `require("net.client")` should unlock HTTP + sockets.
   - `--allow=fs.readonly` should not permit writes.
   - group expansion parity across CLI and script `require`.
2. **Process API behavior**
   - `std.v1.process.run/capture` alias behavior equivalence with `exec_args/exec_capture`.
   - timeout and non-zero exit cases.
3. **FFI typed wrapper**
   - descriptor shape validation errors.
   - parity with raw `ffi_call` on simple known signatures.
4. **`std.v1` module surface**
   - import smoke for `net`, `os`, `signal`, `memory`, `task`, `sync`.
5. **LSP hover checks**
   - builtin hover includes permission/unsafe hints for guarded APIs.
6. **Cross-platform parity**
   - module import and function presence checks on Windows/Linux/macOS.

## Release phasing and rollback

### Release 1
- std.v1 module expansion (`net`, `os`, `signal`, `memory`, `task`, `sync`)
- process/fs v2 aliases
- docs + basic smoke tests

Rollback: keep alias exports additive only; no grammar/runtime breaking changes.

### Release 2
- permissions groups + capability CLI + diagnostics/docs updates
- LSP hover permission hints

Rollback: group/profile expansion is backwards compatible; unknown tokens still pass as raw strings.

### Release 3
- `ffi_call_typed` adoption and wider integration tests
- structured error helper adoption in std modules/tooling

Rollback: retain raw `ffi_call`; typed wrapper is additive.

### Release 4
- hardening pass and CI matrix broadening (permission denial, cross-platform module parity, stress)

Rollback: gate strict checks behind CI profiles until stable.

