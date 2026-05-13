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
13. **System-access wave 30 surface (additive)**:
   - `std.v1.fs`: `fd_open`, `fd_close`, `fd_read`, `fd_write`, `fd_pread`, `fd_pwrite`, `flock`, `statx`, `atomic_write`, `watch`, `space`, `mounts`
   - `std.v1.process`: `spawn_v2`, `wait`, `kill_tree`, `list`, `job_create`, `job_add`, `job_kill`
   - `std.v1.net`: `tcp_server`, `tcp_poll`, `dns_lookup`, `tls_connect`, `ws_connect`, `ws_listen`
   - `std.v1.os`: `runtime_limits`, `features`
   - `std.v1.signal`: `trap`, `untrap`

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

## Release gates (entry / exit)

### Gate R1 (fs + process foundation)
- Entry: `std.v1.fs` low-level fd/stat/atomic APIs exported and documented.
- Exit: coverage contains `test_system_access_30_surface.kn`; build + stable suite green.
- Rollback: keep old `read_file`/`write_file` path untouched and hide only new exports.

### Gate R2 (process lifecycle + unsafe contract)
- Entry: `spawn_v2`/`wait`/`kill_tree`/jobs available.
- Exit: unsafe-path test confirms guarded APIs execute in `unsafe {}` without `require(...)`.
- Rollback: keep legacy `spawn`/`wait_process`/`kill_process` exports as fallback.

### Gate R3 (network expansion)
- Entry: server/poll/dns/tls/ws export surface available in `std.v1.net`.
- Exit: scanner/LSP hints updated for risky network/process APIs.
- Rollback: keep base TCP/UDP/http APIs and disable only new alias exports.

### Gate R4 (observability + tooling)
- Entry: `permissions_active()` includes `granted_with_source`.
- Exit: scanner warns on `process_spawn_v2`, fd APIs, and tls/ws wrappers.
- Rollback: preserve original `permissions_active.granted/groups` fields.

### Gate R5 (packaging controls)
- Entry: capability/lock integration and deterministic controls documented.
- Exit: CI runs source-surface validation in non-game builds.
- Rollback: keep controls optional and non-fatal when absent.

